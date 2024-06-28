#include "render-params.hpp"

Render_params::Draw_parameters Render_params::get_draw_parameters(const Environment& env) const
{
	Draw_parameters out_params;

	glm::mat4 gbuffer_projection;

	// Gbuffer View Paramters
	{
		const auto aspect = (float)env.swapchain.extent.width / env.swapchain.extent.height, fov_y = glm::radians<float>(fov);

		// world -> camera-view
		const glm::mat4 view = camera_controller.view_matrix();

		// camera-view -> camera-projection
		gbuffer_projection = glm::perspective<float>(fov_y, aspect, near, far);

		out_params.eye_position    = camera_controller.eye_position();
		out_params.eye_path        = camera_controller.eye_path();

		// world -> camera-projection
		out_params.view_projection = gbuffer_projection * view;

		out_params.gbuffer_frustum = algorithm::geometry::frustum::Frustum::from_projection(
			out_params.eye_position,
			out_params.eye_path,
			glm::vec3(0, 1, 0),
			aspect,
			fov_y,
			near,
			far
		);
	}

	// directional light
	out_params.light_dir   = get_sunlight_direction();
	out_params.light_color = light_color * light_intensity;

	// Shadow View Params
	{
		// find devision depth
		const auto zdiv_1
			= glm::mix<float>(near * pow(far / near, 1 / 3.0), glm::mix<float>(near, far, 1 / 3.0), csm_blend_factor),
			zdiv_2
			= glm::mix<float>(near * pow(far / near, 2 / 3.0), glm::mix<float>(near, far, 2 / 3.0), csm_blend_factor);

		auto tempz_1 = gbuffer_projection * glm::vec4(0, 0, -zdiv_1, 1),
			 tempz_2 = gbuffer_projection * glm::vec4(0, 0, -zdiv_2, 1);
		tempz_1 /= tempz_1.w, tempz_2 /= tempz_2.w;

		out_params.shadow_div_1 = tempz_1.z;
		out_params.shadow_div_2 = tempz_2.z;

		// world -> old-shadow-view
		const auto shadow_view = glm::lookAt(
			{0, 0, 0},
			-out_params.light_dir,
			out_params.light_dir == glm::vec3(0, 1, 0) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0)
		);

		// camera-projection -> world
		const auto view_projection_inv = glm::inverse(out_params.view_projection);

		// camera-projection -> world -> old-shadow-view
		const auto shadow_transformation = shadow_view * view_projection_inv;

		auto calc_shadow_mat = [=](float depth_min, float depth_max, float near, float far
							   ) -> std::tuple<glm::mat4, algorithm::geometry::frustum::Frustum>
		{
			// generate and transform boundaries
			auto arr = algorithm::geometry::generate_boundaries({-1, -1, depth_min}, {1, 1, depth_max});
			for (auto& v : arr)
			{
				// camera-projection -> old-shadow-view
				auto v4 = shadow_transformation * glm::vec4(v, 1.0);
				v4 /= v4.w;
				v = v4;
			}

			// find convex envelope
			const auto count = algorithm::math::get_convex_envelope(arr);

			// find the smallest area
			float     smallest_area = std::numeric_limits<float>::max();
			float     rotate_ang    = 0;
			float     width, height;
			glm::vec2 center;

			for (auto i : Range(count))
			{
				const auto i2 = (i + 1) % count;                      // index of the next point in the envelope-loop
				const auto p1 = arr[i], p2 = arr[i2];                 // p1: current point; p2: next point
				const auto vec = glm::normalize(glm::vec2(p2 - p1));  // directional unit vector p1->p2

				float min_dot = std::numeric_limits<float>::max(), max_dot = -std::numeric_limits<float>::max(),
					  max_height_sqr = -std::numeric_limits<float>::max();

				// iterate over all points in the envelope
				for (auto j : Range(count))
				{
					const auto vec2 = glm::vec2(arr[j] - p1);  // 2D relative vector

					const auto dot = glm::dot(vec2, vec);  // "horizontal" width
					min_dot        = std::min(min_dot, dot);
					max_dot        = std::max(max_dot, dot);

					const auto height_sqr = glm::length(vec2) * glm::length(vec2) - dot * dot;  // square of the height
					max_height_sqr        = std::max(max_height_sqr, height_sqr);
				}

				// area = width * height = (max_width - min_height) * sqrt(height_square)
				const auto area = std::abs(max_dot - min_dot) * std::sqrt(max_height_sqr);

				// use the smallest area possible
				if (area < smallest_area)
				{
					smallest_area = area;
					rotate_ang    = std::atan2(vec.y, vec.x);
					width         = std::abs(max_dot - min_dot);
					height        = std::sqrt(max_height_sqr);
					center        = glm::vec2(p1) + vec * min_dot;
				}
			}

			// world -> new-shadow-view
			const auto corrected_shadow_view = glm::rotate(glm::mat4(1.0), rotate_ang, {0, 0, -1})
											 * glm::translate(glm::mat4(1.0), glm::vec3(-center, 0.0)) * shadow_view;

			// new-shadow-view -> world
			const auto corrected_shadow_view_inv = glm::inverse(corrected_shadow_view);

			// new-shadow-view -> new-shadow-projection
			const auto shadow_projection = glm::ortho<float>(0, width, 0, height, near, far);

			// world -> new-shadow-projection
			const auto shadow_view_projection = shadow_projection * corrected_shadow_view;

			const auto shadow_frustum = algorithm::geometry::frustum::Frustum::from_ortho(
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

		// DEBUG, show shadow perspective
		if (show_shadow_perspective)
		{
			out_params.view_projection = out_params.shadow_transformations[shadow_perspective_layer];
			out_params.gbuffer_frustum = out_params.shadow_frustums[shadow_perspective_layer];
		}
	}

	return out_params;
}