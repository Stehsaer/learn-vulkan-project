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
		env.debug_marker.set_object_name(render_pass, "Shadow Renderpass");
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
		env.debug_marker.set_object_name(descriptor_set_layout_shadow_matrix, "Shadow Descriptor Set Layout (Shadow Matrix)");
	}

	//* Model Texture DS Layout
	{
		std::array<vk::DescriptorSetLayoutBinding, 2> layout_bindings;

		// layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
		layout_bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		//layout(set = 1, binding = 1) uniform Mat_params
		// {
		// 	float emissive_factor;
		// 	float alpha_cutoff;
		// }
		// mat_params;
		layout_bindings[1]
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		descriptor_set_layout_texture = Descriptor_set_layout(env.device, layout_bindings);
		env.debug_marker.set_object_name(descriptor_set_layout_texture, "Shadow Descriptor Set Layout (Texture)");
	}

	//* Joint Weight DS Layout
	{
		const auto layout_binding
			= vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex);

		descriptor_set_layout_skin = Descriptor_set_layout(env.device, {layout_binding});
		env.debug_marker.set_object_name(descriptor_set_layout_skin, "Shadow Descriptor Set Layout (Skin)");
	}

	//* Pipeline layout
	{
		std::array<vk::PushConstantRange, 1> push_constant_range;

		// layout(push_constant) uniform Model @ VERT
		push_constant_range[0].setOffset(0).setSize(sizeof(Model_matrix)).setStageFlags(vk::ShaderStageFlagBits::eVertex);

		pipeline_layout
			= Pipeline_layout(env.device, {descriptor_set_layout_shadow_matrix, descriptor_set_layout_texture}, push_constant_range);
		env.debug_marker.set_object_name(pipeline_layout, "Shadow Pipeline Layout");

		pipeline_layout_skin = Pipeline_layout(
			env.device,
			{descriptor_set_layout_shadow_matrix, descriptor_set_layout_texture, descriptor_set_layout_skin},
			push_constant_range
		);
		env.debug_marker.set_object_name(pipeline_layout_skin, "Shadow Pipeline Layout (Skin)");
	}

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(shadow_vert), frag_shader = GET_SHADER_MODULE(shadow_frag),
							opaque_vert_shader      = GET_SHADER_MODULE(shadow_opaque_vert),
							skin_vert_shader        = GET_SHADER_MODULE(shadow_skin_vert),
							skin_opaque_vert_shader = GET_SHADER_MODULE(shadow_skin_opaque_vert);

		auto shader_module_infos = std::to_array(
			{vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);
		auto shader_skin_module_infos = std::to_array(
			{skin_vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);
		const auto opaque_module_infos = std::to_array({opaque_vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex)});
		const auto opaque_skin_module_infos = std::to_array({skin_opaque_vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex)});
		create_info.setStages(shader_module_infos);

		// Shader specialization
		struct Spec_map
		{
			VkBool32 alpha_cutoff = false;
			VkBool32 alpha_blend  = false;
		} spec_map;

		const auto constant_entries = std::to_array({
			vk::SpecializationMapEntry{0, offsetof(Spec_map, alpha_cutoff), sizeof(Spec_map::alpha_cutoff)},
			vk::SpecializationMapEntry{1, offsetof(Spec_map, alpha_blend),  sizeof(Spec_map::alpha_blend) },
		});
		const auto specialization_info
			= vk::SpecializationInfo().setDataSize(sizeof(spec_map)).setPData(&spec_map).setMapEntries(constant_entries);

		shader_module_infos[1].setPSpecializationInfo(&specialization_info);
		shader_skin_module_infos[1].setPSpecializationInfo(&specialization_info);

		/* Vertex Input Attribute */

		std::array<vk::VertexInputAttributeDescription, 2> attributes;
		attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);  // in_position
		attributes[1].setBinding(1).setFormat(vk::Format::eR32G32Sfloat).setLocation(1).setOffset(0);     // in_texcoord

		std::array<vk::VertexInputAttributeDescription, 1> opaque_attributes;
		opaque_attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);  // in_position

		std::array<vk::VertexInputAttributeDescription, 4> skin_attributes;
		skin_attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);     // in_position
		skin_attributes[1].setBinding(1).setFormat(vk::Format::eR32G32Sfloat).setLocation(1).setOffset(0);        // in_texcoord
		skin_attributes[2].setBinding(2).setFormat(vk::Format::eR16G16B16A16Uint).setLocation(2).setOffset(0);    // in_joints
		skin_attributes[3].setBinding(3).setFormat(vk::Format::eR32G32B32A32Sfloat).setLocation(3).setOffset(0);  // in_weights

		std::array<vk::VertexInputAttributeDescription, 3> skin_opaque_attributes;
		skin_opaque_attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);     // in_position
		skin_opaque_attributes[1].setBinding(1).setFormat(vk::Format::eR16G16B16A16Uint).setLocation(1).setOffset(0);    // in_joints
		skin_opaque_attributes[2].setBinding(2).setFormat(vk::Format::eR32G32B32A32Sfloat).setLocation(2).setOffset(0);  // in_weights

		/* Vertex Binding */

		std::array<vk::VertexInputBindingDescription, 2> bindings;
		bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));  // in_position
		bindings[1].setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec2));  // in_texcoord

		std::array<vk::VertexInputBindingDescription, 1> opaque_bindings;
		opaque_bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));  // in_position

		std::array<vk::VertexInputBindingDescription, 4> skin_bindings;
		skin_bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));     // in_position
		skin_bindings[1].setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec2));     // in_texcoord
		skin_bindings[2].setBinding(2).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::u16vec4));  // in_joints
		skin_bindings[3].setBinding(3).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec4));     // in_weights

		std::array<vk::VertexInputBindingDescription, 3> skin_opaque_bindings;
		skin_opaque_bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));  // in_position
		skin_opaque_bindings[1].setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::u16vec4));  // in_joints
		skin_opaque_bindings[2].setBinding(2).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec4));  // in_weights

		/* Vertex Input State */

		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
									  .setVertexAttributeDescriptions(attributes)
									  .setVertexBindingDescriptions(bindings);
		auto opaque_vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
											 .setVertexAttributeDescriptions(opaque_attributes)
											 .setVertexBindingDescriptions(opaque_bindings);
		auto skin_vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
										   .setVertexAttributeDescriptions(skin_attributes)
										   .setVertexBindingDescriptions(skin_bindings);
		auto skin_opaque_vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
												  .setVertexAttributeDescriptions(skin_opaque_attributes)
												  .setVertexBindingDescriptions(skin_opaque_bindings);

		create_info.setPVertexInputState(&vertex_input_state);

		/* Input Assembly */

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
									   .setDepthBiasConstantFactor(-0.25)
									   .setDepthBiasSlopeFactor(-0.25);
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

		auto opaque_create_info = create_info;
		opaque_create_info.setStages(opaque_module_infos).setPVertexInputState(&opaque_vertex_input_state);

		auto skin_create_info = create_info;
		skin_create_info.setStages(shader_skin_module_infos)
			.setPVertexInputState(&skin_vertex_input_state)
			.setLayout(pipeline_layout_skin);

		auto skin_opaque_create_info = create_info;
		skin_opaque_create_info.setStages(opaque_skin_module_infos)
			.setPVertexInputState(&skin_opaque_vertex_input_state)
			.setLayout(pipeline_layout_skin);

		//* Single Sided

		// Single sided without skin

		single_side.opaque = Graphics_pipeline(env.device, opaque_create_info);
		env.debug_marker.set_object_name(single_side.opaque, "Shadow Pipeline (Single Sided Opaque)");

		spec_map                    = {true, false};
		single_side.mask            = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(single_side.mask, "Shadow Pipeline (Single Sided Alpha)");

		spec_map                    = {false, true};
		single_side.blend           = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(single_side.blend, "Shadow Pipeline (Single Sided Blend)");

		// Single sided with skin

		single_side_skin.opaque = Graphics_pipeline(env.device, skin_opaque_create_info);
		env.debug_marker.set_object_name(single_side_skin.opaque, "Shadow Pipeline (Single Sided Opaque Skin)");

		spec_map              = {true, false};
		single_side_skin.mask = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(single_side.mask, "Shadow Pipeline (Single Sided Alpha Skin)");

		spec_map               = {false, true};
		single_side_skin.blend = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(single_side_skin.blend, "Shadow Pipeline (Single Sided Blend Skin)");

		//* Double Sided
		rasterization_state.setCullMode(vk::CullModeFlagBits::eNone).setDepthBiasConstantFactor(1.25).setDepthBiasSlopeFactor(1.75);

		// Double sided without skin

		double_side.opaque = Graphics_pipeline(env.device, opaque_create_info);
		env.debug_marker.set_object_name(double_side.opaque, "Shadow Pipeline (Double Sided Opaque)");

		spec_map                    = {true, false};
		double_side.mask            = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(double_side.mask, "Shadow Pipeline (Double Sided Alpha)");

		spec_map                    = {false, true};
		double_side.blend           = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(double_side.blend, "Shadow Pipeline (Double Sided Blend)");

		// Double sided with skin

		double_side_skin.opaque = Graphics_pipeline(env.device, skin_opaque_create_info);
		env.debug_marker.set_object_name(double_side_skin.opaque, "Shadow Pipeline (Double Sided Opaque Skin)");

		spec_map              = {true, false};
		double_side_skin.mask = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(double_side_skin.mask, "Shadow Pipeline (Double Sided Alpha Skin)");

		spec_map               = {false, true};
		double_side_skin.blend = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(double_side_skin.blend, "Shadow Pipeline (Double Sided Blend Skin)");
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
		const auto formats = std::to_array({normal_format, color_format, pbr_format, emissive_format, depth_format});

		for (auto i : Iota(5))
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
		env.debug_marker.set_object_name(render_pass, "Gbuffer Renderpass");
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

	{  // Skin
		const auto layout_binding
			= vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex);

		descriptor_set_layout_skin = Descriptor_set_layout(env.device, {layout_binding});
		env.debug_marker.set_object_name(descriptor_set_layout_skin, "Gbuffer Descriptor Set Layout (Skin)");
	}

	//* Pipeline Layout
	{
		std::array<vk::PushConstantRange, 1> push_constant_range;

		// layout(push_constant) uniform Model @ VERT
		push_constant_range[0].setOffset(0).setSize(sizeof(Model_matrix)).setStageFlags(vk::ShaderStageFlagBits::eVertex);

		auto descriptor_set_layouts = Descriptor_set_layout::to_array({descriptor_set_layout_camera, descriptor_set_layout_texture});

		pipeline_layout = Pipeline_layout(env.device, descriptor_set_layouts, push_constant_range);
		env.debug_marker.set_object_name(pipeline_layout, "Gbuffer Pipeline Layout");

		pipeline_layout_skin = Pipeline_layout(
			env.device,
			{descriptor_set_layout_camera, descriptor_set_layout_texture, descriptor_set_layout_skin},
			push_constant_range
		);
		env.debug_marker.set_object_name(pipeline_layout_skin, "Gbuffer Pipeline Layout (Skin)");
	}

	//* Graphics Pipeline
	{
		vk::GraphicsPipelineCreateInfo create_info;

		// Shaders
		const Shader_module vert_shader = GET_SHADER_MODULE(gbuffer_vert), frag_shader = GET_SHADER_MODULE(gbuffer_frag),
							skin_vert_shader = GET_SHADER_MODULE(gbuffer_skin_vert);

		auto shader_module_infos = std::to_array(
			{vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);
		auto skin_shader_module_infos = std::to_array(
			{skin_vert_shader.stage_info(vk::ShaderStageFlagBits::eVertex), frag_shader.stage_info(vk::ShaderStageFlagBits::eFragment)}
		);
		create_info.setStages(shader_module_infos);

		// Shader specialization
		struct Spec_map
		{
			VkBool32 alpha_cutoff = false;
			VkBool32 alpha_blend  = false;
		} spec_map;

		const auto constant_entries = std::to_array({
			vk::SpecializationMapEntry{0, offsetof(Spec_map, alpha_cutoff), sizeof(Spec_map::alpha_cutoff)},
			vk::SpecializationMapEntry{1, offsetof(Spec_map, alpha_blend),  sizeof(Spec_map::alpha_blend) },
		});
		const auto specialization_info
			= vk::SpecializationInfo().setDataSize(sizeof(spec_map)).setPData(&spec_map).setMapEntries(constant_entries);

		shader_module_infos[1].setPSpecializationInfo(&specialization_info);
		skin_shader_module_infos[1].setPSpecializationInfo(&specialization_info);

		// Vertex Input State

		std::array<vk::VertexInputAttributeDescription, 4> attributes;
		{
			attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);
			attributes[1].setBinding(1).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(1).setOffset(0);
			attributes[2].setBinding(2).setFormat(vk::Format::eR32G32Sfloat).setLocation(2).setOffset(0);
			attributes[3].setBinding(3).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(3).setOffset(0);
		}

		std::array<vk::VertexInputAttributeDescription, 6> skin_attributes;
		{
			skin_attributes[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(0);
			skin_attributes[1].setBinding(1).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(1).setOffset(0);
			skin_attributes[2].setBinding(2).setFormat(vk::Format::eR32G32Sfloat).setLocation(2).setOffset(0);
			skin_attributes[3].setBinding(3).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(3).setOffset(0);
			skin_attributes[4].setBinding(4).setFormat(vk::Format::eR16G16B16A16Uint).setLocation(4).setOffset(0);
			skin_attributes[5].setBinding(5).setFormat(vk::Format::eR32G32B32A32Sfloat).setLocation(5).setOffset(0);
		}

		std::array<vk::VertexInputBindingDescription, 4> bindings;
		{
			bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
			bindings[1].setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
			bindings[2].setBinding(2).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec2));
			bindings[3].setBinding(3).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
		}

		std::array<vk::VertexInputBindingDescription, 6> skin_bindings;
		{
			skin_bindings[0].setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
			skin_bindings[1].setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
			skin_bindings[2].setBinding(2).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec2));
			skin_bindings[3].setBinding(3).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec3));
			skin_bindings[4].setBinding(4).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::u16vec4));
			skin_bindings[5].setBinding(5).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(glm::vec4));
		}

		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
									  .setVertexAttributeDescriptions(attributes)
									  .setVertexBindingDescriptions(bindings);
		auto skin_vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
										   .setVertexAttributeDescriptions(skin_attributes)
										   .setVertexBindingDescriptions(skin_bindings);
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

		auto skin_create_info = create_info;
		skin_create_info.setStages(skin_shader_module_infos)
			.setPVertexInputState(&skin_vertex_input_state)
			.setLayout(pipeline_layout_skin);

		//* Single Sided
		spec_map           = {false, false};
		single_side.opaque = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(single_side.opaque, "Gbuffer Pipeline (Single Sided Opaque)");

		spec_map         = {true, false};
		single_side.mask = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(single_side.mask, "Gbuffer Pipeline (Single Sided Alpha)");

		spec_map          = {false, true};
		single_side.blend = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(single_side.blend, "Gbuffer Pipeline (Single Sided Blend)");

		spec_map                = {false, false};
		single_side_skin.opaque = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(single_side_skin.opaque, "Gbuffer Pipeline (Single Sided Opaque Skin)");

		spec_map              = {true, false};
		single_side_skin.mask = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(single_side_skin.mask, "Gbuffer Pipeline (Single Sided Alpha Skin)");

		spec_map               = {false, true};
		single_side_skin.blend = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(single_side_skin.blend, "Gbuffer Pipeline (Single Sided Blend Skin)");

		//* Double Sided
		rasterization_state.setCullMode(vk::CullModeFlagBits::eNone);

		spec_map           = {false, false};
		double_side.opaque = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(double_side.opaque, "Gbuffer Pipeline (Double Sided Opaque)");

		spec_map         = {true, false};
		double_side.mask = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(double_side.mask, "Gbuffer Pipeline (Double Sided ALpha)");

		spec_map          = {false, true};
		double_side.blend = Graphics_pipeline(env.device, create_info);
		env.debug_marker.set_object_name(double_side.blend, "Gbuffer Pipeline (Double Sided Blend)");

		spec_map                = {false, false};
		double_side_skin.opaque = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(double_side_skin.opaque, "Gbuffer Pipeline (Double Sided Opaque Skin)");

		spec_map              = {true, false};
		double_side_skin.mask = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(double_side_skin.mask, "Gbuffer Pipeline (Double Sided ALpha Skin)");

		spec_map               = {false, true};
		double_side_skin.blend = Graphics_pipeline(env.device, skin_create_info);
		env.debug_marker.set_object_name(double_side_skin.blend, "Gbuffer Pipeline (Double Sided Blend Skin)");
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
		env.debug_marker.set_object_name(render_pass, "Lighting Renderpass");
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
		for (auto i : Iota(7)) bindings[i].setBinding(i);

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
	env.debug_marker.set_object_name(pipeline_layout, "Lighting Pipeline Layout");

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
		env.debug_marker.set_object_name(pipeline, "Lighting Pipeline");
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

		luminance_avg_pipeline_layout
			= Pipeline_layout(env.device, {luminance_avg_descriptor_set_layout}, {luminance_push_constant_range});
		env.debug_marker.set_object_name(luminance_avg_pipeline_layout, "Luminance Avg Pipeline Layout");

		const vk::PushConstantRange lerp_push_constant_range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(Lerp_params));

		lerp_pipeline_layout = Pipeline_layout(env.device, {lerp_descriptor_set_layout}, {lerp_push_constant_range});
		env.debug_marker.set_object_name(lerp_pipeline_layout, "Luminance Lerp Pipeline Layout");
	}

	// Pipeline
	{
		const auto luminance_avg_shader = GET_SHADER_MODULE(luminance_comp), exposure_lerp_shader = GET_SHADER_MODULE(exposure_lerp_comp);

		luminance_avg_pipeline = Compute_pipeline(
			env.device,
			luminance_avg_pipeline_layout,
			luminance_avg_shader.stage_info(vk::ShaderStageFlagBits::eCompute)
		);
		env.debug_marker.set_object_name(luminance_avg_pipeline, "Luminance Avg Pipeline");

		lerp_pipeline
			= Compute_pipeline(env.device, lerp_pipeline_layout, exposure_lerp_shader.stage_info(vk::ShaderStageFlagBits::eCompute));
		env.debug_marker.set_object_name(lerp_pipeline, "Luminance Lerp Pipeline");
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
	env.debug_marker.set_object_name(bloom_filter_pipeline_layout, "Bloom Filter Pipeline Layout");

	bloom_blur_pipeline_layout = Pipeline_layout(env.device, {bloom_blur_descriptor_set_layout}, {});
	env.debug_marker.set_object_name(bloom_blur_pipeline_layout, "BLoom Blur Pipeline Layout");

	bloom_acc_pipeline_layout = Pipeline_layout(env.device, {bloom_acc_descriptor_set_layout}, {});
	env.debug_marker.set_object_name(bloom_acc_pipeline_layout, "Bloom Accumulation Pipeline Layout");

	bloom_filter_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_filter_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_filter_pipeline_layout, stage_info);
	}();
	env.debug_marker.set_object_name(bloom_filter_pipeline, "Bloom Filter Pipeline");

	bloom_blur_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_blur_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_blur_pipeline_layout, stage_info);
	}();
	env.debug_marker.set_object_name(bloom_blur_pipeline, "Bloom Blur Pipeline");

	bloom_acc_pipeline = [=, this]
	{
		const auto shader     = GET_SHADER_MODULE(bloom_acc_comp);
		const auto stage_info = shader.stage_info(vk::ShaderStageFlagBits::eCompute);

		return Compute_pipeline(env.device, bloom_acc_pipeline_layout, stage_info);
	}();
	env.debug_marker.set_object_name(bloom_acc_pipeline, "Bloom Accumulation Pipeline");
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
									.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
									.setFormat(env.swapchain.surface_format.format)
									.setLoadOp(vk::AttachmentLoadOp::eClear)
									.setStoreOp(vk::AttachmentStoreOp::eStore)
									.setSamples(vk::SampleCountFlagBits::e1);

		const vk::AttachmentReference attachment_ref{0, vk::ImageLayout::eColorAttachmentOptimal};

		const auto subpass = vk::SubpassDescription().setColorAttachments(attachment_ref);

		std::array<vk::SubpassDependency, 2> dependencies;

		// External=>S0, Sync Luminance
		dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		// External=>S0, Sync Bloom
		dependencies[1]
			.setSrcSubpass(vk::SubpassExternal)
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setSrcStageMask(vk::PipelineStageFlagBits::eComputeShader)
			.setDstSubpass(0)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);

		render_pass = Render_pass(env.device, {attachment}, {subpass}, {dependencies});
		env.debug_marker.set_object_name(render_pass, "Composite Renderpass");
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
	env.debug_marker.set_object_name(pipeline_layout, "Composite Pipeline Layout");

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
		env.debug_marker.set_object_name(pipeline, "Composite Pipeline");
	}
}

#pragma endregion

#pragma region "Fxaa Pipeline"

static const std::map<Fxaa_mode, const char*> mode_name{
	{Fxaa_mode::No_fxaa,         "No FXAA"          },
	{Fxaa_mode::Fxaa_1,          "FXAA 1.0"         },
	{Fxaa_mode::Fxaa_1_improved, "FXAA 1.0 Improved"},
	{Fxaa_mode::Fxaa_3_quality,  "FXAA 3.0 Quality" },
	{Fxaa_mode::Max_enum,        "MAX ENUM, DEBUG"  }
};

void Fxaa_pipeline::create(const Environment& env)
{
	//* Create Render pass
	render_pass = [=]() -> Render_pass
	{
		const auto attachment = vk::AttachmentDescription()
									.setInitialLayout(vk::ImageLayout::eUndefined)
									.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
									.setFormat(env.swapchain.surface_format.format)
									.setLoadOp(vk::AttachmentLoadOp::eDontCare)
									.setStoreOp(vk::AttachmentStoreOp::eStore)
									.setSamples(vk::SampleCountFlagBits::e1);

		const vk::AttachmentReference attachment_ref{0, vk::ImageLayout::eColorAttachmentOptimal};

		const auto subpass = vk::SubpassDescription().setColorAttachments(attachment_ref);

		std::array<vk::SubpassDependency, 2> dependencies;

		// External=>S0, Sync Composite Output
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

		const auto render_pass = Render_pass(env.device, {attachment}, {subpass}, {dependencies});
		env.debug_marker.set_object_name(render_pass, "Fxaa Renderpass");

		return render_pass;
	}();

	//* Create Descriptor Set
	descriptor_set_layout = [=]() -> Descriptor_set_layout
	{
		std::array<vk::DescriptorSetLayoutBinding, 2> bindings;

		//* Composite Output
		// layout(set = 0, binding = 0) uniform sampler2D input_tex;
		bindings[0]
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		//* Parameters
		// `layout(set = 0, binding = 1) uniform Params`
		bindings[1]
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		const auto descriptor_set_layout = Descriptor_set_layout(env.device, bindings);
		env.debug_marker.set_object_name(descriptor_set_layout, "Fxaa Descriptor Set Layout");

		return descriptor_set_layout;
	}();

	//* Create Pipeline Layout
	pipeline_layout = Pipeline_layout(env.device, {descriptor_set_layout}, {});

	{
		vk::GraphicsPipelineCreateInfo create_info;

		const auto vert = GET_SHADER_MODULE(quad_vert), frag = GET_SHADER_MODULE(fxaa_frag);
		auto       shader_info
			= std::to_array({vert.stage_info(vk::ShaderStageFlagBits::eVertex), frag.stage_info(vk::ShaderStageFlagBits::eFragment)});
		create_info.setStages(shader_info);

		struct
		{
			glm::uint mode;
		} spec;

		const auto constant_entries = std::to_array({
			vk::SpecializationMapEntry{0, offsetof(decltype(spec), mode), sizeof(spec.mode)},
		});
		const auto specialization_info
			= vk::SpecializationInfo().setDataSize(sizeof(spec)).setPData(&spec).setMapEntries(constant_entries);
		shader_info[1].setPSpecializationInfo(&specialization_info);

		// Vertex Input State
		auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();
		create_info.setPVertexInputState(&vertex_input_state);

		// Input Assembly State
		auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleStrip);
		create_info.setPInputAssemblyState(&input_assembly_info);

		// Viewport State
		auto viewports
			= std::to_array({utility::flip_viewport(vk::Viewport(0, 0, env.swapchain.extent.width, env.swapchain.extent.height))});
		auto scissors = std::to_array({vk::Rect2D({0, 0}, env.swapchain.extent)});

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
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
		);

		auto attachment_states = std::to_array({attachment1_blend});
		color_blend_state.setAttachments(attachment_states).setLogicOpEnable(false);
		create_info.setPColorBlendState(&color_blend_state);

		// Dynamic State
		auto                               dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		dynamic_state_info.setDynamicStates(dynamic_states);
		create_info.setPDynamicState(&dynamic_state_info);

		create_info.setRenderPass(render_pass).setLayout(pipeline_layout).setSubpass(0);

		//* Create Graphics Pipeline
		for (const auto mode : Iota((size_t)Fxaa_mode::Max_enum))
		{
			spec.mode       = mode;
			pipelines[mode] = Graphics_pipeline(env.device, create_info);
			env.debug_marker.set_object_name(pipelines[mode], std::format("Fxaa Pipeline ({})", mode_name.at((Fxaa_mode)mode)));
		}
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
	fxaa_pipeline.create(env);
}
