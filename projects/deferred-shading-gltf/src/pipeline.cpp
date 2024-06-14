#include "pipeline.hpp"

#include "binary-resource.hpp"

#define GET_SHADER_MODULE(name) Shader_module(env.device, binary_resource::name##_span)

#pragma region "Shadow Pipeline"

vk::ClearValue Shadow_pipeline::clear_value = vk::ClearDepthStencilValue(1.0, 0);

// Create Shadow Pass Pipeline
void Shadow_pipeline::create(const Environment& env)
{
	//* Render Pass
	{
		const auto attachment_description = vk::AttachmentDescription()
												.setInitialLayout(vk::ImageLayout::eUndefined)
												.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
												.setFormat(shadow_map_format)
												.setLoadOp(vk::AttachmentLoadOp::eClear)
												.setStoreOp(vk::AttachmentStoreOp::eStore)
												.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
												.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
												.setSamples(vk::SampleCountFlagBits::e1);

		const auto depth_attachment = vk::AttachmentReference{0, vk::ImageLayout::eDepthStencilAttachmentOptimal};

		const auto subpass_description = vk::SubpassDescription()
											 .setPColorAttachments(nullptr)
											 .setPDepthStencilAttachment(&depth_attachment)
											 .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 2> subpass_dependencies;

		subpass_dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setDstSubpass(0)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);
		subpass_dependencies[1]
			.setSrcSubpass(0)
			.setDstSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		render_pass = Render_pass(env.device, {attachment_description}, {subpass_description}, subpass_dependencies);
	}

	//* Shadow Matrix DS Layout
	{
		std::array<vk::DescriptorSetLayoutBinding, 1> layout_bindings;

		// layout(set = 0, binding = 0) uniform Camera_uniform @ VERT
		layout_bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);

		descriptor_set_layout_shadow_matrix = Descriptor_set_layout(env.device, layout_bindings);
	}

	//* Model Texture DS Layout
	{
		std::array<vk::DescriptorSetLayoutBinding, 1> layout_bindings;

		// layout(set = 0, binding = 0) uniform Camera_uniform @ VERT
		layout_bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		descriptor_set_layout_texture = Descriptor_set_layout(env.device, layout_bindings);
	}

	//* Pipeline layout
	{
		std::array<vk::PushConstantRange, 1> push_constant_range;

		// layout(push_constant) uniform Model @ VERT
		push_constant_range[0].setOffset(0).setSize(sizeof(Model_matrix)).setStageFlags(vk::ShaderStageFlagBits::eVertex);

		pipeline_layout = Pipeline_layout(env.device, {descriptor_set_layout_shadow_matrix, descriptor_set_layout_texture}, push_constant_range);
	}

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(shadow_vert), frag_shader = GET_SHADER_MODULE(shadow_frag);

		auto shader_module_infos
			= std::to_array({vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)});
		create_info.setStages(shader_module_infos);

		std::array<vk::VertexInputAttributeDescription, 2> attributes;
		attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(offsetof(io::mesh::gltf::vertex, position));
		attributes[1].setBinding(0).setFormat(vk::Format::eR32G32Sfloat).setLocation(1).setOffset(offsetof(io::mesh::gltf::vertex, uv));

		std::array<vk::VertexInputBindingDescription, 1> bindings;
		bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(io::mesh::gltf::vertex));

		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo().setVertexAttributeDescriptions(attributes).setVertexBindingDescriptions(bindings);
		create_info.setPVertexInputState(&vertex_input_state);

		auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
		create_info.setPInputAssemblyState(&input_assembly_info);

		auto viewports      = std::to_array({utility::flip_viewport(vk::Viewport(0, 0, 1024, 1024, 0.0, 1.0))});
		auto scissors       = std::to_array({vk::Rect2D({0, 0}, {1024, 1024})});
		auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewports(viewports).setScissors(scissors);
		create_info.setPViewportState(&viewport_state);

		auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
									   .setPolygonMode(vk::PolygonMode::eFill)
									   .setCullMode(vk::CullModeFlagBits::eFront)
									   .setFrontFace(vk::FrontFace::eCounterClockwise)
									   .setLineWidth(1)
									   .setDepthBiasEnable(true)
									   .setDepthBiasConstantFactor(-1.25)
									   .setDepthBiasSlopeFactor(-1.75);
		create_info.setPRasterizationState(&rasterization_state);

		// Multisample State
		auto multisample_state = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits::e1);
		create_info.setPMultisampleState(&multisample_state);

		// Depth Stencil State
		auto depth_stencil_state
			= vk::PipelineDepthStencilStateCreateInfo().setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(vk::CompareOp::eLess);
		create_info.setPDepthStencilState(&depth_stencil_state);

		// Dynamic State
		auto                               dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		dynamic_state_info.setDynamicStates(dynamic_states);
		create_info.setPDynamicState(&dynamic_state_info);

		// Layout & Renderpass
		create_info.setRenderPass(render_pass).setLayout(pipeline_layout).setSubpass(0);

		pipeline = Graphics_pipeline(env.device, create_info);

		rasterization_state.setCullMode(vk::CullModeFlagBits::eNone).setDepthBiasConstantFactor(1.5).setDepthBiasSlopeFactor(1.75);
		double_sided_pipeline = Graphics_pipeline(env.device, create_info);
	}
}

#pragma endregion

#pragma region "Gbuffer Pipeline"

std::array<vk::ClearValue, 5> Gbuffer_pipeline::clear_values
	= {vk::ClearColorValue(0, 0, 0, 0),
	   vk::ClearColorValue(0, 0, 0, 0),
	   vk::ClearColorValue(0, 0, 0, 0),
	   vk::ClearColorValue(0, 0, 0, 0),
	   vk::ClearDepthStencilValue(1.0, 0)};

// Create Gbuffer Pipeline
void Gbuffer_pipeline::create(const Environment& env)
{
	//* Render Pass
	{
		std::array<vk::AttachmentDescription, 5> attachment_descriptions;
		const auto                               formats = std::to_array({normal_format, color_format, color_format, emissive_format, depth_format});

		for (auto i : Range(5))
			attachment_descriptions[i]
				.setInitialLayout(vk::ImageLayout::eUndefined)
				.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setLoadOp(vk::AttachmentLoadOp::eClear)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setFormat(formats[i])
				.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

		const std::array<vk::AttachmentReference, 4> color_attachment_reference({
			{0, vk::ImageLayout::eColorAttachmentOptimal},
			{1, vk::ImageLayout::eColorAttachmentOptimal},
			{2, vk::ImageLayout::eColorAttachmentOptimal},
			{3, vk::ImageLayout::eColorAttachmentOptimal},
		});

		const vk::AttachmentReference depth_attachment_refeerence{4, vk::ImageLayout::eDepthStencilAttachmentOptimal};

		const auto subpass = vk::SubpassDescription()
								 .setColorAttachments(color_attachment_reference)
								 .setPDepthStencilAttachment(&depth_attachment_refeerence)
								 .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 2> subpass_dependencies;

		// Sync: 0=>External, Color Attachment Sync
		subpass_dependencies[0]
			.setSrcSubpass(0)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(vk::SubpassExternal)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		// Sync: 0=>External, Depth Attachment Sync
		subpass_dependencies[1]
			.setSrcSubpass(0)
			.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
			.setDstSubpass(vk::SubpassExternal)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

		render_pass = Render_pass(env.device, attachment_descriptions, {subpass}, subpass_dependencies);
	}

	//* Descriptor Set Layout

	{  // Camera
		std::array<vk::DescriptorSetLayoutBinding, 1> layout_bindings;

		// layout(set = 0, binding = 0) uniform Camera_uniform @ VERT
		layout_bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);

		descriptor_set_layout_camera = Descriptor_set_layout(env.device, layout_bindings);
	}

	{  // Uniforms
		std::array<vk::DescriptorSetLayoutBinding, 6> layout_bindings;

		// layout(set = 1, binding = 0) uniform sampler2D albedo_texture @ FRAG
		layout_bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// layout(set = 1, binding = 1) uniform sampler2D pbr_texture @ FRAG
		layout_bindings[1]
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// layout(set = 1, binding = 2) uniform sampler2D occlusion_texture;
		layout_bindings[2]
			.setBinding(2)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// layout(set = 1, binding = 3) uniform sampler2D normal_texture;
		layout_bindings[3]
			.setBinding(3)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// layout(set = 1, binding = 4) uniform sampler2D emissive_texture;
		layout_bindings[4]
			.setBinding(4)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// layout(set = 1, binding = 5) uniform Mat_params
		// {
		// 	vec3 emissive_strength;
		// }
		// mat_params;
		layout_bindings[5]
			.setBinding(5)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		descriptor_set_layout_texture = Descriptor_set_layout(env.device, layout_bindings);
	}

	//* Pipeline Layout
	{
		std::array<vk::PushConstantRange, 1> push_constant_range;

		// layout(push_constant) uniform Model @ VERT
		push_constant_range[0].setOffset(0).setSize(sizeof(Model_matrix)).setStageFlags(vk::ShaderStageFlagBits::eVertex);

		auto descriptor_set_layouts = Descriptor_set_layout::to_array({descriptor_set_layout_camera, descriptor_set_layout_texture});

		pipeline_layout = Pipeline_layout(env.device, descriptor_set_layouts, push_constant_range);
	}

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(gbuffer_vert), frag_shader = GET_SHADER_MODULE(gbuffer_frag);

		auto shader_module_infos
			= std::to_array({vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)});
		create_info.setStages(shader_module_infos);

		// Vertex Input State

		std::array<vk::VertexInputAttributeDescription, 4> attributes;
		{
			attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(offsetof(io::mesh::gltf::vertex, position));
			attributes[1].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(1).setOffset(offsetof(io::mesh::gltf::vertex, normal));
			attributes[2].setBinding(0).setFormat(vk::Format::eR32G32Sfloat).setLocation(2).setOffset(offsetof(io::mesh::gltf::vertex, uv));
			attributes[3].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(3).setOffset(offsetof(io::mesh::gltf::vertex, tangent));
		}

		std::array<vk::VertexInputBindingDescription, 1> bindings;

		bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(io::mesh::gltf::vertex));

		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo().setVertexAttributeDescriptions(attributes).setVertexBindingDescriptions(bindings);
		create_info.setPVertexInputState(&vertex_input_state);

		// Input Assembly State
		auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);
		create_info.setPInputAssemblyState(&input_assembly_info);

		// Viewport State
		auto viewports = std::to_array({utility::flip_viewport(vk::Viewport(0, 0, env.swapchain.extent.width, env.swapchain.extent.height, 0.0, 1.0))});
		auto scissors  = std::to_array({vk::Rect2D({0, 0}, env.swapchain.extent)});

		auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewports(viewports).setScissors(scissors);
		create_info.setPViewportState(&viewport_state);

		// Rasterization State
		auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
									   .setPolygonMode(vk::PolygonMode::eFill)
									   .setCullMode(vk::CullModeFlagBits::eBack)
									   .setFrontFace(vk::FrontFace::eCounterClockwise)
									   .setLineWidth(1);
		create_info.setPRasterizationState(&rasterization_state);

		// Multisample State
		auto multisample_state = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits::e1);
		create_info.setPMultisampleState(&multisample_state);

		// Depth Stencil State
		auto depth_stencil_state
			= vk::PipelineDepthStencilStateCreateInfo().setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(vk::CompareOp::eLess);
		create_info.setPDepthStencilState(&depth_stencil_state);

		// Color Blend State
		std::array<vk::PipelineColorBlendAttachmentState, 4> color_blend_attachment_states;

		// Fill attachment state with [NO BLEND, RGBA]
		std::fill(
			color_blend_attachment_states.begin(),
			color_blend_attachment_states.end(),
			vk::PipelineColorBlendAttachmentState().setBlendEnable(false).setColorWriteMask(
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			)
		);

		vk::PipelineColorBlendStateCreateInfo color_blend_state;
		color_blend_state.setAttachments(color_blend_attachment_states).setLogicOpEnable(false);

		create_info.setPColorBlendState(&color_blend_state);

		// Dynamic State
		auto                               dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		dynamic_state_info.setDynamicStates(dynamic_states);
		create_info.setPDynamicState(&dynamic_state_info);

		// Layout & Renderpass
		create_info.setRenderPass(render_pass).setLayout(pipeline_layout).setSubpass(0);

		pipeline = Graphics_pipeline(env.device, create_info);

		rasterization_state.setCullMode(vk::CullModeFlagBits::eNone);
		double_sided_pipeline = Graphics_pipeline(env.device, create_info);
	}
}

#pragma endregion

#pragma region "Lighting Pipeline"

std::array<vk::ClearValue, 2> Lighting_pipeline::clear_value = {vk::ClearColorValue(0, 0, 0, 0), vk::ClearColorValue(0, 0, 0, 0)};

// Create Lighting Pipeline
void Lighting_pipeline::create(const Environment& env)
{
	//* Render Pass
	{
		const auto luminance_attachment = vk::AttachmentDescription()
											  .setFormat(luminance_format)
											  .setInitialLayout(vk::ImageLayout::eUndefined)
											  .setFinalLayout(vk::ImageLayout::eGeneral)
											  .setLoadOp(vk::AttachmentLoadOp::eClear)
											  .setStoreOp(vk::AttachmentStoreOp::eStore)
											  .setSamples(vk::SampleCountFlagBits::e1);

		const auto brightness_attachment = vk::AttachmentDescription()
											   .setFormat(vk::Format::eR16Sfloat)
											   .setInitialLayout(vk::ImageLayout::eUndefined)
											   .setFinalLayout(vk::ImageLayout::eGeneral)
											   .setLoadOp(vk::AttachmentLoadOp::eClear)
											   .setStoreOp(vk::AttachmentStoreOp::eStore)
											   .setSamples(vk::SampleCountFlagBits::e1);

		const auto attachments = std::to_array({luminance_attachment, brightness_attachment});

		const auto attachment_refs = std::to_array<vk::AttachmentReference>({
			{0, vk::ImageLayout::eColorAttachmentOptimal},
			{1, vk::ImageLayout::eColorAttachmentOptimal}
		});

		const auto subpass = vk::SubpassDescription().setColorAttachments(attachment_refs);

		std::array<vk::SubpassDependency, 4> dependencies;

		// External=>S0
		dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		// External=>S0
		dependencies[1]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		// S0=>External
		dependencies[2]
			.setSrcSubpass(0)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(vk::SubpassExternal)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader);

		// External=>S0
		dependencies[3]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
			.setSrcStageMask(vk::PipelineStageFlagBits::eComputeShader)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

		render_pass = Render_pass(env.device, attachments, {subpass}, dependencies);
	}

	//* Descriptor Set Layout
	{  // Gbuffer Input Layout, set = 0
		std::array<vk::DescriptorSetLayoutBinding, 7> bindings;

		// Fill bindings 0~5
		std::fill(
			bindings.begin(),
			bindings.begin() + 6,
			vk::DescriptorSetLayoutBinding()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		);

		// Assign binding indices
		for (auto i : Range(7)) bindings[i].setBinding(i);

		// Shadow map
		bindings[4].setDescriptorCount(csm_count);

		// Uniform buffers
		bindings[6].setBinding(6).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

		gbuffer_input_layout = Descriptor_set_layout(env.device, bindings);
	}
	{  // Skybox Input Layout, set = 1
		const auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>({
			{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
			{1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
			{2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
		});

		skybox_input_layout = Descriptor_set_layout(env.device, bindings);
	}

	//* Pipeline Layout
	pipeline_layout = Pipeline_layout(env.device, {gbuffer_input_layout, skybox_input_layout}, {});

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(quad_vert), frag_shader = GET_SHADER_MODULE(lighting_frag);

		auto shader_module_infos
			= std::to_array({vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)});
		create_info.setStages(shader_module_infos);

		// Vertex Input State
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		create_info.setPVertexInputState(&vertex_input_state);

		// Input Assembly State
		auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleStrip);
		create_info.setPInputAssemblyState(&input_assembly_info);

		// Viewport State
		auto viewports = std::to_array({utility::flip_viewport(vk::Viewport(0, 0, env.swapchain.extent.width, env.swapchain.extent.height))});
		auto scissors  = std::to_array({vk::Rect2D({0, 0}, env.swapchain.extent)});

		auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewports(viewports).setScissors(scissors);
		create_info.setPViewportState(&viewport_state);

		// Rasterization State
		auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
									   .setPolygonMode(vk::PolygonMode::eFill)
									   .setCullMode(vk::CullModeFlagBits::eNone)
									   .setFrontFace(vk::FrontFace::eClockwise)
									   .setLineWidth(1);
		create_info.setPRasterizationState(&rasterization_state);

		// Multisample State
		auto multisample_state = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits::e1);
		create_info.setPMultisampleState(&multisample_state);

		// Depth Stencil State
		auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo().setDepthTestEnable(false);
		create_info.setPDepthStencilState(&depth_stencil_state);

		// Color Blend State
		vk::PipelineColorBlendStateCreateInfo color_blend_state;

		vk::PipelineColorBlendAttachmentState attachment1_blend;
		attachment1_blend.setBlendEnable(false).setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		);

		vk::PipelineColorBlendAttachmentState attachment2_blend;
		attachment2_blend.setBlendEnable(false).setColorWriteMask(vk::ColorComponentFlagBits::eR);
		// attachment1_blend.setBlendEnable(true)
		// 	.setColorBlendOp(vk::BlendOp::eAdd)
		// 	.setAlphaBlendOp(vk::BlendOp::eAdd)
		// 	.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
		// 	.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
		// 	.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
		// 	.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
		// 	.setColorWriteMask(
		// 		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
		// 		| vk::ColorComponentFlagBits::eA
		// 	);

		auto attachment_states = std::to_array({attachment1_blend, attachment2_blend});
		color_blend_state.setAttachments(attachment_states).setLogicOpEnable(false);
		create_info.setPColorBlendState(&color_blend_state);

		// Dynamic State
		auto                               dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		dynamic_state_info.setDynamicStates(dynamic_states);
		create_info.setPDynamicState(&dynamic_state_info);

		// Layout & Renderpass
		create_info.setRenderPass(render_pass).setLayout(pipeline_layout).setSubpass(0);

		//* Create Graphics Pipeline
		pipeline = Graphics_pipeline(env.device, create_info);
	}
}

#pragma endregion

#pragma region "Auto Exposure Pipeline"

// Create Auto Exposure Pipeline
void Auto_exposure_compute_pipeline::create(const Environment& env)
{
	// Descriptor Set Layout
	{
		const std::array<vk::DescriptorSetLayoutBinding, 2> luminance_avg_layout_bindings{
			{{0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
			 {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}}
		};

		luminance_avg_descriptor_set_layout = Descriptor_set_layout(env.device, luminance_avg_layout_bindings);

		const std::array<vk::DescriptorSetLayoutBinding, 2> lerp_layout_bindings{
			{{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
			 {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}}
		};

		lerp_descriptor_set_layout = Descriptor_set_layout(env.device, lerp_layout_bindings);
	}

	// Pipeline Layout
	{
		const vk::PushConstantRange luminance_push_constant_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(Luminance_params));

		luminance_avg_pipeline_layout = Pipeline_layout(env.device, {luminance_avg_descriptor_set_layout}, {luminance_push_constant_range});

		const vk::PushConstantRange lerp_push_constant_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(Lerp_params));

		lerp_pipeline_layout = Pipeline_layout(env.device, {lerp_descriptor_set_layout}, {lerp_push_constant_range});
	}

	// Pipeline
	{
		const auto luminance_avg_shader = GET_SHADER_MODULE(luminance_comp), exposure_lerp_shader = GET_SHADER_MODULE(exposure_lerp_comp);

		luminance_avg_pipeline
			= Compute_pipeline(env.device, luminance_avg_pipeline_layout, luminance_avg_shader.stage_info(vk::ShaderStageFlagBits::eCompute));

		lerp_pipeline = Compute_pipeline(env.device, lerp_pipeline_layout, exposure_lerp_shader.stage_info(vk::ShaderStageFlagBits::eCompute));
	}
}

#pragma endregion

#pragma region "Bloom Pipeline"

void Bloom_pipeline::create(const Environment& env)
{
	bloom_filter_descriptor_set_layout = [=]
	{
		const std::array<vk::DescriptorSetLayoutBinding, 3> bindings({
			{0, vk::DescriptorType::eStorageImage,  1, vk::ShaderStageFlagBits::eCompute},
			{1, vk::DescriptorType::eStorageImage,  1, vk::ShaderStageFlagBits::eCompute},
			{2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute}
		});

		return Descriptor_set_layout(env.device, bindings);
	}();

	bloom_blur_descriptor_set_layout = [=]
	{
		const std::array<vk::DescriptorSetLayoutBinding, 2> bindings({
			{0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
			{1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute}
		});

		return Descriptor_set_layout(env.device, bindings);
	}();

	bloom_acc_descriptor_set_layout = [=]
	{
		const std::array<vk::DescriptorSetLayoutBinding, 3> bindings({
			{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
			{1, vk::DescriptorType::eStorageImage,         1, vk::ShaderStageFlagBits::eCompute},
			{2, vk::DescriptorType::eStorageImage,         1, vk::ShaderStageFlagBits::eCompute}
		});

		return Descriptor_set_layout(env.device, bindings);
	}();

	bloom_filter_pipeline_layout = [=, this]
	{
		const vk::PushConstantRange push_constant_range{vk::ShaderStageFlagBits::eCompute, 0, sizeof(Params)};
		return Pipeline_layout(env.device, {bloom_filter_descriptor_set_layout}, {push_constant_range});
	}();

	bloom_blur_pipeline_layout = Pipeline_layout(env.device, {bloom_blur_descriptor_set_layout}, {});

	bloom_acc_pipeline_layout = Pipeline_layout(env.device, {bloom_acc_descriptor_set_layout}, {});

	bloom_filter_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_filter_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_filter_pipeline_layout, stage_info);
	}();

	bloom_blur_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_blur_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_blur_pipeline_layout, stage_info);
	}();

	bloom_acc_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_acc_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_acc_pipeline_layout, stage_info);
	}();
}

#pragma endregion

#pragma region "Composite Pipeline"

vk::ClearValue Composite_pipeline::clear_value = vk::ClearColorValue(0, 0, 0, 0);

// Create Composite Pipeline
void Composite_pipeline::create(const Environment& env)
{
	//* Render Pass
	{
		const auto attachment = vk::AttachmentDescription()
									.setInitialLayout(vk::ImageLayout::eUndefined)
									.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
									.setFormat(env.swapchain.surface_format.format)
									.setLoadOp(vk::AttachmentLoadOp::eClear)
									.setStoreOp(vk::AttachmentStoreOp::eStore)
									.setSamples(vk::SampleCountFlagBits::e1);

		const vk::AttachmentReference attachment_ref{0, vk::ImageLayout::eColorAttachmentOptimal};

		const auto subpass = vk::SubpassDescription().setColorAttachments(attachment_ref);

		std::array<vk::SubpassDependency, 3> dependencies;

		// External=>S0, Sync Luminance
		dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		// S0=>External, Sync IMGUI
		dependencies[1]
			.setSrcSubpass(0)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(vk::SubpassExternal)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

		// External=>S0, Sync Bloom
		dependencies[2]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eComputeShader)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		render_pass = Render_pass(env.device, {attachment}, {subpass}, {dependencies});
	}

	//* Descriptor Set Layout
	{
		std::array<vk::DescriptorSetLayoutBinding, 4> bindings;

		// Luminance
		bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		// Parameters
		bindings[1].setBinding(1).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

		bindings[2].setBinding(2).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

		bindings[3]
			.setBinding(3)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		descriptor_set_layout = Descriptor_set_layout(env.device, bindings);
	}

	//* Pipeline Layout
	pipeline_layout = Pipeline_layout(env.device, {descriptor_set_layout}, {});

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(quad_vert), frag_shader = GET_SHADER_MODULE(composite_frag);

		auto shader_module_infos
			= std::to_array({vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)});
		create_info.setStages(shader_module_infos);

		// Vertex Input State
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		create_info.setPVertexInputState(&vertex_input_state);

		// Input Assembly State
		auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleStrip);
		create_info.setPInputAssemblyState(&input_assembly_info);

		// Viewport State
		auto viewports = std::to_array({utility::flip_viewport(vk::Viewport(0, 0, env.swapchain.extent.width, env.swapchain.extent.height))});
		auto scissors  = std::to_array({vk::Rect2D({0, 0}, env.swapchain.extent)});

		auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewports(viewports).setScissors(scissors);
		create_info.setPViewportState(&viewport_state);

		// Rasterization State
		auto rasterization_state = vk::PipelineRasterizationStateCreateInfo()
									   .setPolygonMode(vk::PolygonMode::eFill)
									   .setCullMode(vk::CullModeFlagBits::eNone)
									   .setFrontFace(vk::FrontFace::eClockwise)
									   .setLineWidth(1);
		create_info.setPRasterizationState(&rasterization_state);

		// Multisample State
		auto multisample_state = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits::e1);
		create_info.setPMultisampleState(&multisample_state);

		// Depth Stencil State
		auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo().setDepthTestEnable(false);
		create_info.setPDepthStencilState(&depth_stencil_state);

		// Color Blend State
		vk::PipelineColorBlendStateCreateInfo color_blend_state;

		vk::PipelineColorBlendAttachmentState attachment1_blend;
		attachment1_blend.setBlendEnable(false).setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		);

		auto attachment_states = std::to_array({attachment1_blend});
		color_blend_state.setAttachments(attachment_states).setLogicOpEnable(false);
		create_info.setPColorBlendState(&color_blend_state);

		// Dynamic State
		auto                               dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		dynamic_state_info.setDynamicStates(dynamic_states);
		create_info.setPDynamicState(&dynamic_state_info);

		// Layout & Renderpass
		create_info.setRenderPass(render_pass).setLayout(pipeline_layout).setSubpass(0);

		//* Create Graphics Pipeline
		pipeline = Graphics_pipeline(env.device, create_info);
	}
}

#pragma endregion

void Pipeline_set::create(const Environment& env)
{
	shadow_pipeline.create(env);
	gbuffer_pipeline.create(env);
	lighting_pipeline.create(env);
	auto_exposure_pipeline.create(env);
	bloom_pipeline.create(env);
	composite_pipeline.create(env);
}
