#include "render-params.hpp"

void Render_source::generate_material_data(const Environment& env, const Pipeline_set& pipeline)
{
	const auto& materials     = model->materials;
	const auto& texture_views = model->texture_views;
	material_data.clear();

	// no material present, not usual
	if (materials.empty()) return;

	// Alignment calculation
	const auto min_alignment = env.physical_device.getProperties().limits.minUniformBufferOffsetAlignment;
	const auto element_size
		= sizeof(io::mesh::gltf::Material::Mat_params) + min_alignment - sizeof(io::mesh::gltf::Material::Mat_params) % min_alignment;

	//* Create Descriptor Pool.
	// Gbuffer set: 5 Combined + 1 Uniform;
	// Shadow set: 1 Combined + 1 Uniform;
	// Total: 6 Combined + 2 Uniform.
	{
		const auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
			{vk::DescriptorType::eCombinedImageSampler, 6},
			{vk::DescriptorType::eUniformBuffer,        2}
		});

		descriptor_pool = Descriptor_pool(env.device, descriptor_pool_size, materials.size() * 2);
	}

	// Create Descriptor Sets
	const auto layouts
		= std::vector<vk::DescriptorSetLayout>(materials.size(), pipeline.gbuffer_pipeline.descriptor_set_layout_texture);
	const auto layouts_albedo_only
		= std::vector<vk::DescriptorSetLayout>(materials.size(), pipeline.shadow_pipeline.descriptor_set_layout_texture);
	const auto descriptor_set_list             = Descriptor_set::create_multiple(env.device, descriptor_pool, layouts);
	const auto albedo_only_descriptor_set_list = Descriptor_set::create_multiple(env.device, descriptor_pool, layouts_albedo_only);

	// Create uniform buffer
	material_uniform_buffer = Buffer(
		env.allocator,
		element_size * materials.size(),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_GPU_ONLY
	);
	{  // Upload uniform buffer data
		const auto staging_buffer = Buffer(
			env.allocator,
			element_size * materials.size(),
			vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		std::vector<uint8_t> mat_params(element_size * materials.size());
		for (const auto i : Range(materials.size()))
		{
			auto* ptr                                                     = mat_params.data() + element_size * i;
			*reinterpret_cast<io::mesh::gltf::Material::Mat_params*>(ptr) = materials[i].params;
		}
		staging_buffer << mat_params;

		const Command_pool   pool(env.device, env.g_family_idx);
		const Command_buffer cmd(pool);

		cmd.begin(true);
		cmd.copy_buffer(material_uniform_buffer, staging_buffer, 0, 0, element_size * materials.size());
		cmd.end();

		const auto submit_buffers = Command_buffer::to_array({cmd});
		env.t_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_buffers));
		env.t_queue.waitIdle();
	}

	// Iterates over all materials
	for (const auto i : Range(materials.size()))
	{
		const auto& material       = materials[i];
		const auto &descriptor_set = descriptor_set_list[i], &albedo_only_set = albedo_only_descriptor_set_list[i];

		// Write Descriptor Sets
		const auto descriptor_image_write_info = std::to_array<vk::DescriptorImageInfo>(
			{texture_views[material.albedo_idx].descriptor_info(),
			 texture_views[material.metal_roughness_idx].descriptor_info(),
			 texture_views[material.occlusion_idx].descriptor_info(),
			 texture_views[material.normal_idx].descriptor_info(),
			 texture_views[material.emissive_idx].descriptor_info()}
		);

		const auto descriptor_uniform_write_info
			= vk::DescriptorBufferInfo(material_uniform_buffer, element_size * i, sizeof(io::mesh::gltf::Material::Mat_params));

		std::array<vk::WriteDescriptorSet, 6> normal_write_info;
		std::array<vk::WriteDescriptorSet, 2> albedo_only_write_info;

		// Albedo
		normal_write_info[0]
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 0)
			.setDstSet(descriptor_set);
		// Metal-roughness
		normal_write_info[1]
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 1)
			.setDstSet(descriptor_set);
		// Occlusion
		normal_write_info[2]
			.setDstBinding(2)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 2)
			.setDstSet(descriptor_set);
		// Normal
		normal_write_info[3]
			.setDstBinding(3)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 3)
			.setDstSet(descriptor_set);
		// Emissive
		normal_write_info[4]
			.setDstBinding(4)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 4)
			.setDstSet(descriptor_set);
		// Material Param
		normal_write_info[5]
			.setDstBinding(5)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setPBufferInfo(&descriptor_uniform_write_info)
			.setDstSet(descriptor_set);

		albedo_only_write_info[0]
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setPImageInfo(descriptor_image_write_info.data() + 0)
			.setDstSet(albedo_only_set);
		albedo_only_write_info[1]
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setPBufferInfo(&descriptor_uniform_write_info)
			.setDstSet(albedo_only_set);

		const auto combined_write_info = utility::join_array(normal_write_info, albedo_only_write_info);

		env.device->updateDescriptorSets(combined_write_info, {});

		material_data.emplace_back(descriptor_set, albedo_only_set);
	}
}

static glm::vec3 get_sunlight_direction(float sunlight_yaw, float sunlight_pitch)
{
	auto mat = glm::rotate(
		glm::rotate(glm::mat4(1.0), glm::radians<float>(sunlight_yaw), glm::vec3(0, 1, 0)),
		glm::radians<float>(sunlight_pitch),
		glm::vec3(0, 0, 1)
	);

	auto light = mat * glm::vec4(1, 0, 0, 0);
	return glm::normalize(glm::vec3(light));
}

Render_params::Runtime_parameters Render_params::gen_runtime_parameters(const Environment& env) const
{
	Runtime_parameters out_params;

	glm::mat4 gbuffer_projection;

	// Gbuffer View Paramters
	{
		const auto aspect = (float)env.swapchain.extent.width / env.swapchain.extent.height, fov_y = glm::radians<float>(fov);

		// world -> camera-view
		const glm::mat4 view = camera_controller.view_matrix();

		// camera-view -> camera-projection
		gbuffer_projection = glm::perspective<float>(fov_y, aspect, near, far);

		out_params.eye_position = camera_controller.eye_position();
		out_params.eye_path     = camera_controller.eye_path();

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
	out_params.light_dir   = get_sunlight_direction(sunlight_yaw, sunlight_pitch);
	out_params.light_color = light_color * light_intensity;

	// Shadow View Params
	{
		// find devision depth
		const auto zdiv_1 = glm::mix<float>(near * pow(far / near, 1 / 3.0), glm::mix<float>(near, far, 1 / 3.0), csm_blend_factor),
				   zdiv_2 = glm::mix<float>(near * pow(far / near, 2 / 3.0), glm::mix<float>(near, far, 2 / 3.0), csm_blend_factor);

		auto tempz_1 = gbuffer_projection * glm::vec4(0, 0, -zdiv_1, 1), tempz_2 = gbuffer_projection * glm::vec4(0, 0, -zdiv_2, 1);
		tempz_1 /= tempz_1.w, tempz_2 /= tempz_2.w;

		const auto shadow_div_1 = tempz_1.z, shadow_div_2 = tempz_2.z;

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
							   ) -> std::tuple<glm::mat4, algorithm::geometry::frustum::Frustum, glm::vec2>
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

			return {
				shadow_view_projection, // shadow transformation
				shadow_frustum, // shadow frustum
				{width, height}  // shadow view cube 2d size
			};
		};

		std::tie(out_params.shadow_transformations[0], out_params.shadow_frustums[0], out_params.shadow_view_sizes[0])
			= calc_shadow_mat(0, shadow_div_1, shadow_near[0], shadow_far[0]);

		std::tie(out_params.shadow_transformations[1], out_params.shadow_frustums[1], out_params.shadow_view_sizes[1])
			= calc_shadow_mat(shadow_div_1, shadow_div_2, shadow_near[1], shadow_far[1]);

		std::tie(out_params.shadow_transformations[2], out_params.shadow_frustums[2], out_params.shadow_view_sizes[2])
			= calc_shadow_mat(shadow_div_2, 1, shadow_near[2], shadow_far[2]);

		// DEBUG, show shadow perspective
		if (show_shadow_perspective)
		{
			out_params.view_projection = out_params.shadow_transformations[shadow_perspective_layer];
			out_params.gbuffer_frustum = out_params.shadow_frustums[shadow_perspective_layer];
		}
	}

	return out_params;
}