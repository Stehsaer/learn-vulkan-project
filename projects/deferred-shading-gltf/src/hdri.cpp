#include "hdri.hpp"
#include "binary-resource.hpp"

void Hdri_resource::create_sampler(const Environment& env)
{
	bilinear_sampler = [=]
	{
		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
											 .setAnisotropyEnable(false)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(1)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		return Image_sampler(env.device, sampler_create_info);
	}();

	mipmapped_sampler = [=]
	{
		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
											 .setAnisotropyEnable(false)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(environment_layers)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		return Image_sampler(env.device, sampler_create_info);
	}();

	lut_sampler = [=]
	{
		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
											 .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
											 .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
											 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
											 .setAnisotropyEnable(false)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(1)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		return Image_sampler(env.device, sampler_create_info);
	}();
}

void Hdri_resource::generate_environment(const Environment& env, const Image_view& input_image, uint32_t resolution)
{
	const Render_pass render_pass = [=]
	{
		std::array<vk::AttachmentDescription, 2> attachment_descriptions;

		// one face of hdri output
		attachment_descriptions[0]
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setFormat(vk::Format::eR16G16B16A16Sfloat)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore);

		// mipmapped hdri output
		attachment_descriptions[1]
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eTransferSrcOptimal)
			.setFormat(vk::Format::eR16G16B16A16Sfloat)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore);

		std::array<vk::AttachmentReference, 2> subpass0_attachment_ref;
		subpass0_attachment_ref[0].setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		subpass0_attachment_ref[1].setAttachment(1).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		std::array<vk::SubpassDescription, 1> subpass_descriptions;
		subpass_descriptions[0]
			.setColorAttachments(subpass0_attachment_ref)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 1> subpass_dependencies;
		subpass_dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstSubpass(0)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		return Render_pass(env.device, attachment_descriptions, subpass_descriptions, subpass_dependencies);
	}();

	/* Create Layout Objects */

	const Descriptor_set_layout descriptor_set_layout = [=]
	{
		const auto layout_bindings = std::to_array<vk::DescriptorSetLayoutBinding>({vk::DescriptorSetLayoutBinding(
			0,
			vk::DescriptorType::eCombinedImageSampler,
			1,
			vk::ShaderStageFlagBits::eFragment
		)});

		return Descriptor_set_layout(env.device, layout_bindings);
	}();

	const Pipeline_layout pipeline_layout = [=]
	{
		const auto push_constants
			= std::to_array({vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(Push_constant))});
		const auto descriptor_set_layouts = Descriptor_set_layout::to_array({descriptor_set_layout});

		return Pipeline_layout(env.device, descriptor_set_layouts, push_constants);
	}();

	/* Create Descriptors */

	const Descriptor_pool descriptor_pool = [=]
	{
		const auto pool_sizes = std::to_array<vk::DescriptorPoolSize>({
			{vk::DescriptorType::eCombinedImageSampler, 1}
		});

		return Descriptor_pool(env.device, pool_sizes, 1);
	}();

	const Descriptor_set descriptor_set = [=]
	{
		const auto layouts = Descriptor_set_layout::to_array({descriptor_set_layout});
		return Descriptor_set::create_multiple(env.device, descriptor_pool, layouts)[0];
	}();

	// Update Descriptor Sets
	{
		std::array<vk::DescriptorImageInfo, 1> image_infos;
		image_infos[0]
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setImageView(input_image)
			.setSampler(bilinear_sampler);

		std::array<vk::WriteDescriptorSet, 1> write_infos;
		write_infos[0]
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(image_infos)
			.setDstBinding(0)
			.setDstSet(descriptor_set);

		env.device->updateDescriptorSets(write_infos, {});
	}

	/* Create Images & Image Views */

	std::array<Image_view, 6> cubemap_image_views, mipmapped_image_views;

	{  // Environment & Specular IBL
		environment = Image(
			env.allocator,
			vk::ImageType::e2D,
			{resolution, resolution, 1},
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			environment_layers,
			vk::ImageLayout::eUndefined,
			vk::SampleCountFlagBits::e1,
			6,
			vk::ImageCreateFlagBits::eCubeCompatible
		);

		environment_view = Image_view(
			env.device,
			environment,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::eCube,
			{vk::ImageAspectFlagBits::eColor, 0, environment_layers, 0, 6}
		);

		for (auto i : Range(6))
			cubemap_image_views[i] = Image_view(
				env.device,
				environment,
				vk::Format::eR16G16B16A16Sfloat,
				vk::ImageViewType::e2D,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, i, 1)
			);
	}

	const uint32_t temp_mipmapped_env_level = algorithm::texture::log2_mipmap_level(resolution, resolution);

	{  // Temporary Mipmapped Environment
		mipmapped_environment = Image(
			env.allocator,
			vk::ImageType::e2D,
			{resolution, resolution, 1},
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
				| vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			temp_mipmapped_env_level,
			vk::ImageLayout::eUndefined,
			vk::SampleCountFlagBits::e1,
			6,
			vk::ImageCreateFlagBits::eCubeCompatible
		);

		mipmapped_environment_view = Image_view(
			env.device,
			mipmapped_environment,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::eCube,
			{vk::ImageAspectFlagBits::eColor, 0, temp_mipmapped_env_level, 0, 6}
		);

		for (auto i : Range(6))
			mipmapped_image_views[i] = Image_view(
				env.device,
				mipmapped_environment,
				vk::Format::eR16G16B16A16Sfloat,
				vk::ImageViewType::e2D,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, i, 1)
			);

		mipmapped_environment_sampler = [=]
		{
			const auto sampler_create_info = vk::SamplerCreateInfo()
												 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
												 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
												 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
												 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
												 .setAnisotropyEnable(false)
												 .setCompareEnable(false)
												 .setMinLod(0)
												 .setMaxLod(temp_mipmapped_env_level)
												 .setMinFilter(vk::Filter::eLinear)
												 .setMagFilter(vk::Filter::eLinear)
												 .setUnnormalizedCoordinates(false);

			return Image_sampler(env.device, sampler_create_info);
		}();
	}

	/* Create Framebuffers */

	std::array<Framebuffer, 6> framebuffers;
	for (auto i : Range(6))
	{
		const auto image_views = Image_view::to_array({cubemap_image_views[i], mipmapped_image_views[i]});
		framebuffers[i]        = Framebuffer(env.device, render_pass, image_views, {resolution, resolution, 1});
	}

	/* Create Pipeline */
	const Graphics_pipeline pipeline = [=]
	{
		vk::GraphicsPipelineCreateInfo pipeline_create_info;

		/* Load Shaders */
		auto vert_module = Shader_module(env.device, binary_resource::cube_vert_span),
			 frag_module = Shader_module(env.device, binary_resource::gen_cubemap_frag_span);

		const auto stage_infos = std::to_array(
			{vert_module.stage_info(vk::ShaderStageFlagBits::eVertex),
			 frag_module.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);

		pipeline_create_info.setStages(stage_infos);

		/* Setup Vertex Input: No vertex input */
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		pipeline_create_info.setPVertexInputState(&vertex_input_state);

		/* Setup Input Assembly */
		const auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo()
											  .setTopology(vk::PrimitiveTopology::eTriangleList)
											  .setPrimitiveRestartEnable(false);
		pipeline_create_info.setPInputAssemblyState(&input_assembly_state);

		/* Setup Viewport & Scissor */
		const auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		pipeline_create_info.setPViewportState(&viewport_state);

		/* Setup Rasterization */
		const auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
											 .setDepthClampEnable(false)
											 .setRasterizerDiscardEnable(false)
											 .setPolygonMode(vk::PolygonMode::eFill)
											 .setLineWidth(1.0f)
											 .setCullMode(vk::CullModeFlagBits::eNone)
											 .setFrontFace(vk::FrontFace::eCounterClockwise)
											 .setDepthBiasEnable(false);
		pipeline_create_info.setPRasterizationState(&rasterization_state);

		/* Setup Multisampling */
		const auto multisample_state = vk::PipelineMultisampleStateCreateInfo()
										   .setRasterizationSamples(vk::SampleCountFlagBits::e1)
										   .setSampleShadingEnable(false);
		pipeline_create_info.setPMultisampleState(&multisample_state);

		/* Setup Color Blending */
		const auto color_blend_attachment = vk::PipelineColorBlendAttachmentState()
												.setColorWriteMask(
													vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
													| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
												)
												.setBlendEnable(false);
		const auto color_blend_attachments = std::to_array({color_blend_attachment, color_blend_attachment});

		const auto color_blend_state = vk::PipelineColorBlendStateCreateInfo().setAttachments(color_blend_attachments);
		pipeline_create_info.setPColorBlendState(&color_blend_state);

		/* Setup Dynamic State */
		const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		const auto dynamic_state  = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);
		pipeline_create_info.setPDynamicState(&dynamic_state);

		/* Setup Layouts */
		pipeline_create_info.setRenderPass(render_pass);
		pipeline_create_info.setLayout(pipeline_layout);
		pipeline_create_info.setSubpass(0);

		return Graphics_pipeline(env.device, pipeline_create_info);
	}();

	// 6 view matrices
	const auto view_matrices = std::to_array<glm::mat4>(
		{glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))}
	);
	const auto view_projection = glm::perspective(glm::radians<float>(90), 1.0f, 0.1f, 10.0f);

	/* Render */
	const auto command_buffer = Command_buffer(env.command_pool);
	{
		command_buffer.begin();

		const auto clear_values
			= std::to_array<vk::ClearValue>({vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})});

		command_buffer.set_viewport(utility::flip_viewport(vk::Viewport(0, 0, resolution, resolution, 0, 1)));
		command_buffer.set_scissor({
			{0,          0         },
			{resolution, resolution}
		});

		// Draw 6 faces
		for (auto i : Range(6))
		{
			command_buffer.begin_render_pass(
				render_pass,
				framebuffers[i],
				vk::Rect2D({0, 0}, {resolution, resolution}),
				clear_values,
				vk::SubpassContents::eInline
			);
			{
				const auto push_constant = Push_constant{view_projection * view_matrices[i]};

				command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, pipeline);
				command_buffer
					.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {descriptor_set});

				command_buffer.push_constants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constant);

				command_buffer.draw(0, 36, 0, 1);
			}
			command_buffer.end_render_pass();
		}

		/* Create Mipmap */
		algorithm::texture::generate_mipmap(
			command_buffer,
			mipmapped_environment,
			resolution,
			resolution,
			temp_mipmapped_env_level,
			0,
			6,
			vk::ImageLayout::eTransferSrcOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal
		);

		command_buffer.end();

		// Submit Command Buffer
		const auto command_buffers = command_buffer.raw();
		env.g_queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffers));
	}

	env.g_queue.waitIdle();
}

void Hdri_resource::generate_descriptors(const Device& device, const Descriptor_set_layout& layout)
{
	const auto descriptor_pool_sizes = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eCombinedImageSampler, 3}
	});

	descriptor_pool = Descriptor_pool(device, descriptor_pool_sizes, 8);

	descriptor_set = Descriptor_set::create_multiple(device, descriptor_pool, {layout})[0];

	// Write descriptor sets

	// Skybox
	const vk::DescriptorImageInfo environment_image_info{
		mipmapped_sampler,
		environment_view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	// Diffuse
	const vk::DescriptorImageInfo diffuse_image_info{
		bilinear_sampler,
		diffuse_view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	// Diffuse
	const vk::DescriptorImageInfo brdf_lut_image_info{
		lut_sampler,
		brdf_lut_view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	const vk::WriteDescriptorSet write_environment_set{
		descriptor_set,
		0,
		0,
		1,
		vk::DescriptorType::eCombinedImageSampler,
		&environment_image_info
	};

	const vk::WriteDescriptorSet
		write_diffuse_set{descriptor_set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &diffuse_image_info};

	const vk::WriteDescriptorSet
		write_lut_set{descriptor_set, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &brdf_lut_image_info};

	device->updateDescriptorSets({write_environment_set, write_diffuse_set, write_lut_set}, {});
}

void Hdri_resource::generate_diffuse(const Environment& env, uint32_t resolution)
{
	const Render_pass render_pass = [=]
	{
		std::array<vk::AttachmentDescription, 1> attachment_descriptions;

		// one face of hdri output
		attachment_descriptions[0]
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setFormat(vk::Format::eR16G16B16A16Sfloat)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore);

		std::array<vk::AttachmentReference, 1> subpass0_attachment_ref;
		subpass0_attachment_ref[0].setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		std::array<vk::SubpassDescription, 1> subpass_descriptions;
		subpass_descriptions[0]
			.setColorAttachments(subpass0_attachment_ref)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 1> subpass_dependencies;
		subpass_dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstSubpass(0)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		return Render_pass(env.device, attachment_descriptions, subpass_descriptions, subpass_dependencies);
	}();

	/* Create Layout Objects */

	const Descriptor_set_layout descriptor_set_layout = [=]
	{
		const auto layout_bindings = std::to_array<vk::DescriptorSetLayoutBinding>({vk::DescriptorSetLayoutBinding(
			0,
			vk::DescriptorType::eCombinedImageSampler,
			1,
			vk::ShaderStageFlagBits::eFragment
		)});

		return Descriptor_set_layout(env.device, layout_bindings);
	}();

	const Pipeline_layout pipeline_layout = [=]
	{
		const auto push_constants
			= std::to_array({vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(Push_constant))});
		const auto descriptor_set_layouts = Descriptor_set_layout::to_array({descriptor_set_layout});

		return Pipeline_layout(env.device, descriptor_set_layouts, push_constants);
	}();

	/* Create Descriptors */

	const Descriptor_pool descriptor_pool = [=]
	{
		const auto pool_sizes = std::to_array<vk::DescriptorPoolSize>({
			{vk::DescriptorType::eCombinedImageSampler, 1}
		});

		return Descriptor_pool(env.device, pool_sizes, 1);
	}();

	const Descriptor_set descriptor_set = [=]
	{
		const auto layouts = Descriptor_set_layout::to_array({descriptor_set_layout});
		return Descriptor_set::create_multiple(env.device, descriptor_pool, layouts)[0];
	}();

	// Update Descriptor Sets
	{
		std::array<vk::DescriptorImageInfo, 1> image_infos;
		image_infos[0]
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setImageView(mipmapped_environment_view)
			.setSampler(mipmapped_environment_sampler);

		std::array<vk::WriteDescriptorSet, 1> write_infos;
		write_infos[0]
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(image_infos)
			.setDstBinding(0)
			.setDstSet(descriptor_set);

		env.device->updateDescriptorSets(write_infos, {});
	}

	std::array<Image_view, 6> cubemap_image_views;
	{
		diffuse = Image(
			env.allocator,
			vk::ImageType::e2D,
			{resolution, resolution, 1},
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			1,
			vk::ImageLayout::eUndefined,
			vk::SampleCountFlagBits::e1,
			6,
			vk::ImageCreateFlagBits::eCubeCompatible
		);

		diffuse_view = Image_view(
			env.device,
			diffuse,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::eCube,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6}
		);

		for (auto i : Range(6))
			cubemap_image_views[i] = Image_view(
				env.device,
				diffuse,
				vk::Format::eR16G16B16A16Sfloat,
				vk::ImageViewType::e2D,
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, i, 1)
			);
	}

	/* Create Framebuffers */

	std::array<Framebuffer, 6> framebuffers;
	for (auto i : Range(6))
	{
		framebuffers[i] = Framebuffer(env.device, render_pass, {cubemap_image_views[i]}, {resolution, resolution, 1});
	}

	const Graphics_pipeline pipeline = [=]
	{
		vk::GraphicsPipelineCreateInfo pipeline_create_info;

		/* Load Shaders */
		auto vert_module = Shader_module(env.device, binary_resource::cube_vert_span),
			 frag_module = Shader_module(env.device, binary_resource::gen_diffuse_frag_span);

		const auto stage_infos = std::to_array(
			{vert_module.stage_info(vk::ShaderStageFlagBits::eVertex),
			 frag_module.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);

		pipeline_create_info.setStages(stage_infos);

		/* Setup Vertex Input: No vertex input */
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		pipeline_create_info.setPVertexInputState(&vertex_input_state);

		/* Setup Input Assembly */
		const auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo()
											  .setTopology(vk::PrimitiveTopology::eTriangleList)
											  .setPrimitiveRestartEnable(false);
		pipeline_create_info.setPInputAssemblyState(&input_assembly_state);

		/* Setup Viewport & Scissor */
		const auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		pipeline_create_info.setPViewportState(&viewport_state);

		/* Setup Rasterization */
		const auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
											 .setDepthClampEnable(false)
											 .setRasterizerDiscardEnable(false)
											 .setPolygonMode(vk::PolygonMode::eFill)
											 .setLineWidth(1.0f)
											 .setCullMode(vk::CullModeFlagBits::eNone)
											 .setFrontFace(vk::FrontFace::eCounterClockwise)
											 .setDepthBiasEnable(false);
		pipeline_create_info.setPRasterizationState(&rasterization_state);

		/* Setup Multisampling */
		const auto multisample_state = vk::PipelineMultisampleStateCreateInfo()
										   .setRasterizationSamples(vk::SampleCountFlagBits::e1)
										   .setSampleShadingEnable(false);
		pipeline_create_info.setPMultisampleState(&multisample_state);

		/* Setup Color Blending */
		const auto color_blend_attachment = vk::PipelineColorBlendAttachmentState()
												.setColorWriteMask(
													vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
													| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
												)
												.setBlendEnable(false);
		const auto color_blend_state
			= vk::PipelineColorBlendStateCreateInfo().setAttachmentCount(1).setPAttachments(&color_blend_attachment);
		pipeline_create_info.setPColorBlendState(&color_blend_state);

		/* Setup Dynamic State */
		const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		const auto dynamic_state  = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);
		pipeline_create_info.setPDynamicState(&dynamic_state);

		/* Setup Layouts */
		pipeline_create_info.setRenderPass(render_pass);
		pipeline_create_info.setLayout(pipeline_layout);
		pipeline_create_info.setSubpass(0);

		return Graphics_pipeline(env.device, pipeline_create_info);
	}();

	// 6 view matrices
	const auto view_matrices = std::to_array<glm::mat4>(
		{glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))}
	);
	const auto view_projection = glm::perspective(glm::radians<float>(90), 1.0f, 0.1f, 10.0f);

	/* Render */
	const auto command_buffer = Command_buffer(env.command_pool);
	{
		command_buffer.begin();

		const auto clear_values
			= std::to_array<vk::ClearValue>({vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})});

		command_buffer.set_viewport(utility::flip_viewport(vk::Viewport(0, 0, resolution, resolution, 0, 1)));
		command_buffer.set_scissor({
			{0,          0         },
			{resolution, resolution}
		});

		// Draw 6 faces
		for (auto i : Range(6))
		{
			command_buffer.begin_render_pass(
				render_pass,
				framebuffers[i],
				vk::Rect2D({0, 0}, {resolution, resolution}),
				clear_values,
				vk::SubpassContents::eInline
			);
			{
				const auto push_constant = Push_constant{view_projection * view_matrices[i]};

				command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, pipeline);
				command_buffer
					.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {descriptor_set});

				command_buffer.push_constants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constant);

				command_buffer.draw(0, 36, 0, 1);
			}
			command_buffer.end_render_pass();
		}

		command_buffer.end();

		// Submit Command Buffer
		const auto command_buffers = command_buffer.raw();
		env.g_queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffers));
	}

	env.g_queue.waitIdle();
}

void Hdri_resource::generate_specular(const Environment& env, uint32_t resolution)
{
	struct Gen_params
	{
		float roughness;
		float resolution;
	};

	const Render_pass render_pass = [=]
	{
		std::array<vk::AttachmentDescription, 1> attachment_descriptions;

		// one face of hdri output
		attachment_descriptions[0]
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setFormat(vk::Format::eR16G16B16A16Sfloat)
			.setLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStoreOp(vk::AttachmentStoreOp::eStore);

		std::array<vk::AttachmentReference, 1> subpass0_attachment_ref;
		subpass0_attachment_ref[0].setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		std::array<vk::SubpassDescription, 1> subpass_descriptions;
		subpass_descriptions[0]
			.setColorAttachments(subpass0_attachment_ref)
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 1> subpass_dependencies;
		subpass_dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstSubpass(0)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		return Render_pass(env.device, attachment_descriptions, subpass_descriptions, subpass_dependencies);
	}();

	/* Create Layout Objects */

	const Descriptor_set_layout descriptor_set_layout = [=]
	{
		const auto layout_bindings = std::to_array<vk::DescriptorSetLayoutBinding>({vk::DescriptorSetLayoutBinding(
			0,
			vk::DescriptorType::eCombinedImageSampler,
			1,
			vk::ShaderStageFlagBits::eFragment
		)});

		return Descriptor_set_layout(env.device, layout_bindings);
	}();

	const Pipeline_layout pipeline_layout = [=]
	{
		const auto push_constants = std::to_array(
			{vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(Push_constant)),
			 vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, sizeof(Push_constant), sizeof(Gen_params))}
		);

		const auto descriptor_set_layouts = Descriptor_set_layout::to_array({descriptor_set_layout});

		return Pipeline_layout(env.device, descriptor_set_layouts, push_constants);
	}();

	/* Create Descriptors */

	const Descriptor_pool descriptor_pool = [=]
	{
		const auto pool_sizes = std::to_array<vk::DescriptorPoolSize>({
			{vk::DescriptorType::eCombinedImageSampler, 1}
		});

		return Descriptor_pool(env.device, pool_sizes, 1);
	}();

	const Descriptor_set descriptor_set = [=]
	{
		const auto layouts = Descriptor_set_layout::to_array({descriptor_set_layout});
		return Descriptor_set::create_multiple(env.device, descriptor_pool, layouts)[0];
	}();

	// Update Descriptor Sets
	{
		std::array<vk::DescriptorImageInfo, 1> image_infos;
		image_infos[0]
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			.setImageView(mipmapped_environment_view)
			.setSampler(mipmapped_environment_sampler);

		std::array<vk::WriteDescriptorSet, 1> write_infos;
		write_infos[0]
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(image_infos)
			.setDstBinding(0)
			.setDstSet(descriptor_set);

		env.device->updateDescriptorSets(write_infos, {});
	}

	const Graphics_pipeline pipeline = [=]
	{
		vk::GraphicsPipelineCreateInfo pipeline_create_info;

		/* Load Shaders */
		auto vert_module = Shader_module(env.device, binary_resource::cube_vert_span),
			 frag_module = Shader_module(env.device, binary_resource::gen_specular_frag_span);

		const auto stage_infos = std::to_array(
			{vert_module.stage_info(vk::ShaderStageFlagBits::eVertex),
			 frag_module.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);

		pipeline_create_info.setStages(stage_infos);

		/* Setup Vertex Input: No vertex input */
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		pipeline_create_info.setPVertexInputState(&vertex_input_state);

		/* Setup Input Assembly */
		const auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo()
											  .setTopology(vk::PrimitiveTopology::eTriangleList)
											  .setPrimitiveRestartEnable(false);
		pipeline_create_info.setPInputAssemblyState(&input_assembly_state);

		/* Setup Viewport & Scissor */
		const auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
		pipeline_create_info.setPViewportState(&viewport_state);

		/* Setup Rasterization */
		const auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
											 .setDepthClampEnable(false)
											 .setRasterizerDiscardEnable(false)
											 .setPolygonMode(vk::PolygonMode::eFill)
											 .setLineWidth(1.0f)
											 .setCullMode(vk::CullModeFlagBits::eNone)
											 .setFrontFace(vk::FrontFace::eCounterClockwise)
											 .setDepthBiasEnable(false);
		pipeline_create_info.setPRasterizationState(&rasterization_state);

		/* Setup Multisampling */
		const auto multisample_state = vk::PipelineMultisampleStateCreateInfo()
										   .setRasterizationSamples(vk::SampleCountFlagBits::e1)
										   .setSampleShadingEnable(false);
		pipeline_create_info.setPMultisampleState(&multisample_state);

		/* Setup Color Blending */
		const auto color_blend_attachment = vk::PipelineColorBlendAttachmentState()
												.setColorWriteMask(
													vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
													| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
												)
												.setBlendEnable(false);
		const auto color_blend_state
			= vk::PipelineColorBlendStateCreateInfo().setAttachmentCount(1).setPAttachments(&color_blend_attachment);
		pipeline_create_info.setPColorBlendState(&color_blend_state);

		/* Setup Dynamic State */
		const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		const auto dynamic_state  = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);
		pipeline_create_info.setPDynamicState(&dynamic_state);

		/* Setup Layouts */
		pipeline_create_info.setRenderPass(render_pass);
		pipeline_create_info.setLayout(pipeline_layout);
		pipeline_create_info.setSubpass(0);

		return Graphics_pipeline(env.device, pipeline_create_info);
	}();

	// 6 view matrices
	const auto view_matrices = std::to_array<glm::mat4>(
		{glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		 glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))}
	);
	const auto view_projection = glm::perspective(glm::radians<float>(90), 1.0f, 0.1f, 10.0f);

	// create image_view & framebuffers
	std::vector<std::array<Framebuffer, 6>> framebuffers(environment_layers - 1);
	std::vector<std::array<Image_view, 6>>  image_views(environment_layers - 1);
	std::vector<uint32_t>                   resolution_list(environment_layers - 1);

	// Generate resolution list
	{
		uint32_t resolution_downsampled = resolution;
		for (auto level : Range(environment_layers - 1))
		{
			resolution_downsampled /= 2;
			resolution_list[level] = resolution_downsampled;
		}
	}

	// Generate Framebuffers and Image views
	for (auto level : Range(environment_layers - 1))
	{
		for (auto face : Range(6u))
		{
			image_views[level][face] = Image_view(
				env.device,
				environment,
				vk::Format::eR16G16B16A16Sfloat,
				vk::ImageViewType::e2D,
				{vk::ImageAspectFlagBits::eColor, level + 1, 1, face, 1}
			);

			framebuffers[level][face] = Framebuffer(
				env.device,
				render_pass,
				{image_views[level][face]},
				{resolution_list[level], resolution_list[level], 1}
			);
		}
	}

	const auto command_buffer = Command_buffer(env.command_pool);
	{
		command_buffer.begin();

		// draw each level
		for (auto level : Range(environment_layers - 1))
		{
			const auto current_resolution = resolution_list[level];
			command_buffer.set_viewport(
				utility::flip_viewport(vk::Viewport(0, 0, current_resolution, current_resolution, 0, 1))
			);
			command_buffer.set_scissor({
				{0,                  0                 },
				{current_resolution, current_resolution}
			});

			// Draw 6 faces
			for (auto i : Range(6))
			{
				command_buffer.begin_render_pass(
					render_pass,
					framebuffers[level][i],
					vk::Rect2D({0, 0}, {current_resolution, current_resolution}),
					{},
					vk::SubpassContents::eInline
				);
				{
					const auto push_constant = Push_constant{view_projection * view_matrices[i]};
					const auto gen_params    = Gen_params{(float)(level + 1) / environment_layers, (float)resolution};

					command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, pipeline);
					command_buffer
						.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {descriptor_set});

					command_buffer.push_constants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constant, 0);
					command_buffer.push_constants(
						pipeline_layout,
						vk::ShaderStageFlagBits::eFragment,
						gen_params,
						sizeof(Push_constant)
					);

					command_buffer.draw(0, 36, 0, 1);
				}
				command_buffer.end_render_pass();
			}
		}

		command_buffer.end();

		// Submit Command Buffer
		const auto command_buffers = command_buffer.raw();
		env.g_queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffers));
	}

	env.g_queue.waitIdle();
}

void Hdri_resource::generate_brdf_lut(const Environment& env, uint32_t resolution)
{
	const auto descriptor_set_layout = [=]
	{
		const auto bindings = std::to_array(
			{vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute)}
		);

		return Descriptor_set_layout(env.device, bindings);
	}();

	const auto pipeline_layout = Pipeline_layout(env.device, {descriptor_set_layout}, {});

	const auto compute_pipeline = [=]
	{
		const auto shader_module = Shader_module(env.device, binary_resource::gen_brdf_comp_span);

		return Compute_pipeline(
			env.device,
			pipeline_layout,
			shader_module.stage_info(vk::ShaderStageFlagBits::eCompute)
		);
	}();

	const auto descriptor_pool = [=]
	{
		const auto sizes = std::to_array({vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1)});
		return Descriptor_pool(env.device, sizes, 1);
	}();

	const auto descriptor_set
		= Descriptor_set::create_multiple(env.device, descriptor_pool, {descriptor_set_layout})[0];

	// Create Images
	std::tie(brdf_lut, brdf_lut_view) = [=]
	{
		const auto img = Image(
			env.allocator,
			vk::ImageType::e2D,
			{resolution, resolution, 1},
			vk::Format::eR16G16Unorm,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		const auto view = Image_view(
			env.device,
			img,
			vk::Format::eR16G16Unorm,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		return std::tuple(img, view);
	}();

	// Update Descriptor Set
	{
		const vk::DescriptorImageInfo write_img_info{{}, brdf_lut_view, vk::ImageLayout::eGeneral};
		const vk::WriteDescriptorSet
			write_info{descriptor_set, 0, 0, 1, vk::DescriptorType::eStorageImage, &write_img_info};

		env.device->updateDescriptorSets({write_info}, {});
	}

	const auto command_buffer = Command_buffer(env.command_pool);
	{
		command_buffer.begin(true);

		// Transit to eGeneral
		command_buffer.layout_transit(
			brdf_lut,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
			{},
			vk::AccessFlagBits::eShaderWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eComputeShader,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		const auto dispatch_size = (uint32_t)(ceil((float)resolution / 16));

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);
		command_buffer.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_set});
		command_buffer->dispatch(dispatch_size, dispatch_size, 1);

		// Transit to eShaderReadOnlyOptimal
		command_buffer.layout_transit(
			brdf_lut,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eFragmentShader,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		command_buffer.end();

		const auto command_buffer_list = Command_buffer::to_array({command_buffer});
		env.c_queue.submit(vk::SubmitInfo().setCommandBuffers(command_buffer_list));
	}

	env.c_queue.waitIdle();
}