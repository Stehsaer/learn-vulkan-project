#include "render-params.hpp"

#include <memory_resource>
#include <stack>

static std::tuple<uint32_t, std::array<glm::vec3, 8>> get_convex(const std::array<glm::vec3, 8>& input)
{
	struct Point
	{
		glm::vec3 pt;
		float     angle = 0;
	};

	std::array<glm::vec3, 8> output = input;
	std::array<Point, 8>     points;

	// avoid allocating on heap
	std::array<uint32_t, 8>             stack_memory_pool;
	std::pmr::monotonic_buffer_resource pool(stack_memory_pool.data(), stack_memory_pool.size() * sizeof(uint32_t));
	std::pmr::vector<uint32_t>          stack(&pool);

	// find smallest y
	for (auto i : Range(1, 8))
		if (output[i].y < output[0].y || (output[i].y == output[0].y && output[i].x < output[0].x))
			std::swap(output[i], output[0]);

	// calculate angle
	points[0] = Point{output[0]};
	for (auto i : Range(1, 8))
	{
		const glm::vec2 delta = output[i] - output[0];
		points[i]             = Point{output[i], std::atan2(delta.y, delta.x)};
	}

	// sort
	std::sort(
		points.begin() + 1,
		points.end(),
		[](auto pt1, auto pt2)
		{
			return pt1.angle < pt2.angle;
		}
	);

	stack.push_back(0);
	stack.push_back(1);

	for (auto i : Range(2, 8))
	{
		while (stack.size() >= 2)
		{
			const auto delta1 = points[*stack.rbegin()].pt - points[*(stack.rbegin() + 1)].pt;
			const auto delta2 = points[i].pt - points[*stack.rbegin()].pt;

			if (delta1.x * delta2.y - delta2.x * delta1.y <= 0)
				stack.pop_back();
			else
				break;
		}

		stack.push_back(i);
	}

	const uint32_t convex_count = stack.size();

	for (auto i : Range(convex_count))
	{
		output[i] = points[stack[i]].pt;
	}

	return {convex_count, output};
}

Render_params::Draw_parameters Render_params::get_draw_parameters(const Environment& env) const
{
	Draw_parameters out_params;

	glm::mat4 gbuffer_projection;

	// Gbuffer View Paramters
	{
		const auto aspect = (float)env.swapchain.extent.width / env.swapchain.extent.height, fov_y = glm::radians<float>(fov);

		const glm::mat4 view = camera_controller.view_matrix();
		gbuffer_projection   = glm::perspective<float>(fov_y, aspect, near, far);

		out_params.eye_position    = camera_controller.eye_position();
		out_params.view_projection = gbuffer_projection * view;
		out_params.eye_path        = camera_controller.eye_path();

		out_params.gbuffer_frustum = algorithm::frustum_culling::Frustum::from_projection(
			out_params.eye_position,
			out_params.eye_path,
			glm::vec3(0, 1, 0),
			aspect,
			fov_y,
			near,
			far
		);
	}

	out_params.light_dir   = get_sunlight_direction();
	out_params.light_color = light_color * light_intensity;

	// Shadow View Params
	{
		const auto zdiv_1
			= glm::mix<float>(near * pow(far / near, 1 / 3.0), glm::mix<float>(near, far, 1 / 3.0), csm_blend_factor),
			zdiv_2
			= glm::mix<float>(near * pow(far / near, 2 / 3.0), glm::mix<float>(near, far, 2 / 3.0), csm_blend_factor);

		auto tempz_1 = gbuffer_projection * glm::vec4(0, 0, -zdiv_1, 1),
			 tempz_2 = gbuffer_projection * glm::vec4(0, 0, -zdiv_2, 1);
		tempz_1 /= tempz_1.w, tempz_2 /= tempz_2.w;

		out_params.shadow_div_1 = tempz_1.z;
		out_params.shadow_div_2 = tempz_2.z;

		const auto shadow_view = glm::lookAt(
			{0, 0, 0},
			-out_params.light_dir,
			out_params.light_dir == glm::vec3(0, 1, 0) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0)
		);
		const auto view_projection_inv   = glm::inverse(out_params.view_projection);
		const auto shadow_transformation = shadow_view * view_projection_inv;

		auto calc_shadow_mat = [=](float depth_min, float depth_max, float near, float far
							   ) -> std::tuple<glm::mat4, algorithm::frustum_culling::Frustum>
		{
			auto vertices = algorithm::generate_boundaries({-1, -1, depth_min}, {1, 1, depth_max});
			for (auto& v : vertices)
			{
				auto v4 = shadow_transformation * glm::vec4(v, 1.0);
				v4 /= v4.w;
				v = v4;
			}

			// find convex
			const auto [count, arr] = get_convex(vertices);

			// find the smallest area
			float     smallest_area = std::numeric_limits<float>::max();
			float     rotate_ang    = 0;
			float     width, height;
			glm::vec2 center;

			for (auto i : Range(count))
			{
				const auto i2 = (i + 1) % count;
				const auto p1 = arr[i], p2 = arr[i2];
				const auto vec = glm::normalize(glm::vec2(p2 - p1));

				float min_dot = std::numeric_limits<float>::max(), max_dot = -std::numeric_limits<float>::max(),
					  max_height_sqr = -std::numeric_limits<float>::max();

				for (auto j : Range(count))
				{
					const auto vec2 = glm::vec2(arr[j] - p1);

					const auto dot = glm::dot(vec2, vec);
					min_dot        = std::min(min_dot, dot);
					max_dot        = std::max(max_dot, dot);

					const auto height_sqr = glm::length(vec2) * glm::length(vec2) - dot * dot;
					max_height_sqr        = std::max(max_height_sqr, height_sqr);
				}

				const auto area = std::abs(max_dot - min_dot) * std::sqrt(max_height_sqr);

				if (area < smallest_area)
				{
					smallest_area = area;
					rotate_ang    = std::atan2(vec.y, vec.x);
					width         = std::abs(max_dot - min_dot);
					height        = std::sqrt(max_height_sqr);
					center        = glm::vec2(p1) + vec * min_dot;
				}
			}

			const auto corrected_shadow_view = glm::rotate(glm::mat4(1.0), rotate_ang, {0, 0, -1})
											 * glm::translate(glm::mat4(1.0), glm::vec3(-center, 0.0)) * shadow_view;
			const auto corrected_shadow_view_inv = glm::inverse(corrected_shadow_view);
			const auto shadow_projection         = glm::ortho<float>(0, width, 0, height, near, far);
			const auto shadow_view_projection    = shadow_projection * corrected_shadow_view;
			const auto shadow_frustum            = algorithm::frustum_culling::Frustum::from_ortho(
                glm::vec3(corrected_shadow_view_inv * glm::vec4(0.0, 0.0, 0.0, 1.0)),
                glm::vec3(corrected_shadow_view_inv * glm::vec4(0.0, 0.0, -1.0, 0.0)),
                glm::vec3(corrected_shadow_view_inv * glm::vec4(0.0, 1.0, 0.0, 0.0)),
                0,
                width,
                0,
                height,
                near,
                far
            );

			return {shadow_view_projection, shadow_frustum};
		};

		std::tie(out_params.shadow_transformations[0], out_params.shadow_frustums[0])
			= calc_shadow_mat(0, out_params.shadow_div_1, shadow_near[0], shadow_far[0]);

		std::tie(out_params.shadow_transformations[1], out_params.shadow_frustums[1])
			= calc_shadow_mat(out_params.shadow_div_1, out_params.shadow_div_2, shadow_near[1], shadow_far[1]);

		std::tie(out_params.shadow_transformations[2], out_params.shadow_frustums[2])
			= calc_shadow_mat(out_params.shadow_div_2, 1, shadow_near[2], shadow_far[2]);

		// DEBUG
		if (show_shadow_perspective)
		{
			out_params.view_projection = out_params.shadow_transformations[shadow_perspective_layer];
			out_params.gbuffer_frustum = out_params.shadow_frustums[shadow_perspective_layer];
		}
	}

	return out_params;
}