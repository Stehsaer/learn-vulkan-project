#include "application.hpp"

void Deferred_pipeline::create_layouts(const App_environment& env, const App_swapchain& swapchain)
{
	// Descriptor set layout
	{
		// Binding for Camera Uniform, binding = 0 @ vert
		VkDescriptorSetLayoutBinding camera_uniform_binding{
			.binding         = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = swapchain.image_count,
			.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT
		};

		// Binding for albedo textures, binding = 1 @ frag
		VkDescriptorSetLayoutBinding albedo_texture_uniform_binding{
			.binding         = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = swapchain.image_count,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT

		};

		// Binding for pbr textures, binding = 2 @ frag
		VkDescriptorSetLayoutBinding pbr_texture_uniform_binding{
			.binding         = 2,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = swapchain.image_count,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
		};

		auto bindings
			= std::to_array({camera_uniform_binding, albedo_texture_uniform_binding, pbr_texture_uniform_binding});

		auto result = Descriptor_set_layout::create(env.device, bindings) >> descriptor_set_layout;
		CHECK_RESULT(result, "Create Descriptor Set for Deferred Pipeline Failed");
	}

	// Pipeline layout
	{
		auto descriptor_set_layouts = std::to_array({*descriptor_set_layout});

		auto push_ranges = std::to_array({
			VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(Model)}
		});

		auto result = Pipeline_layout::create(env.device, descriptor_set_layouts, push_ranges) >> pipeline_layout;
		CHECK_RESULT(result, "Create Pipeline Layout for Deferred Pipeline Failed");
	}
}

void Deferred_pipeline::create_descriptors(const App_environment& env, const App_swapchain& swapchain)
{
	// Create Descriptor Pool
	{
		// 1 uniform
		VkDescriptorPoolSize uniform_pool_size{
			.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = swapchain.image_count
		};

		// 2 samplers
		VkDescriptorPoolSize sampler_pool_size{
			.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = swapchain.image_count * 2
		};

		auto pool_sizes = std::to_array({uniform_pool_size, sampler_pool_size});

		auto result = Descriptor_pool::create(env.device, pool_sizes, swapchain.image_count) >> descriptor_pool;
	}

	// Create Descriptor Sets
	{
		std::vector<VkDescriptorSetLayout> layouts(swapchain.image_count, *descriptor_set_layout);

		auto result
			= Descriptor_set::allocate_from(descriptor_pool, swapchain.image_count, layouts) >> descriptor_set_list;
		CHECK_RESULT(result, "Create Descriptor Set for Deferred Pipeline Failed");
	}
}

void Deferred_pipeline::create_pipeline(
	const App_environment&        env,
	const App_swapchain&          swapchain,
	const Deferred_render_target& render_target
)
{
	/* ====== Shader Modules ====== */

	Shader_module vertex_shader, fragment_shader;
	{
		auto vert_bin = tools::read_binary("shaders/gbuffer.vert.spv"),
			 frag_bin = tools::read_binary("shaders/gbuffer.frag.spv");

		CHECK_RESULT(vert_bin, "Read Shader Binary shaders/gbuffer.vert.spv Failed");
		CHECK_RESULT(frag_bin, "Read Shader Binary shaders/gbuffer.frag.spv Failed");

		CHECK_RESULT(
			Shader_module::create(env.device, vert_bin.val()) >> vertex_shader,
			"Create Shader Module (Gbuffer Vertex) Failed"
		);
		CHECK_RESULT(
			Shader_module::create(env.device, frag_bin.val()) >> fragment_shader,
			"Create Shader Module (Gbuffer Vertex) Failed"
		);
	}
	auto shader_stages = std::to_array(
		{vertex_shader.stage_info(VK_SHADER_STAGE_VERTEX_BIT), fragment_shader.stage_info(VK_SHADER_STAGE_FRAGMENT_BIT)}
	);

	/* ====== Vertex Input State ====== */

	VkVertexInputBindingDescription per_vertex_binding{
		.binding   = 0,
		.stride    = sizeof(Vertex_data),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	auto vertex_attributes = Vertex_data::get_vertex_input_attribute(0);

	VkPipelineVertexInputStateCreateInfo vertex_input_state{
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = &per_vertex_binding,
		.vertexAttributeDescriptionCount = vertex_attributes.size(),
		.pVertexAttributeDescriptions    = vertex_attributes.data()
	};

	/* ======  Input Assembly State ====== */
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
		.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	/* ====== Viewport State ====== */

	VkViewport viewport = {0, 0, (float)swapchain.extent.width, (float)swapchain.extent.height};
	tools::flip_viewport(viewport);
	VkRect2D scissor = {
		{0, 0},
		swapchain.extent
	};

	VkPipelineViewportStateCreateInfo viewport_state{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = &viewport,
		.scissorCount  = 1,
		.pScissors     = &scissor
	};

	/* ====== Rasterization State ====== */
	VkPipelineRasterizationStateCreateInfo rasterization_state{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable        = false,
		.rasterizerDiscardEnable = false,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.cullMode                = VK_CULL_MODE_BACK_BIT,
		.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable         = false,
		.lineWidth               = 1
	};

	/* ====== Multisample State ====== */
	VkPipelineMultisampleStateCreateInfo multisample_state{
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	/* ====== Depth Stencil State ====== */
	auto depth_stencil_state = utility_values::default_depth_test_enabled;

	/* ====== Color Blend State ====== */

	auto blend_state        = utility_values::default_color_blend_state;
	blend_state.blendEnable = false;

	// Disable blend for all deferred rendering render targets
	auto blend_states = std::to_array({blend_state, blend_state, blend_state, blend_state});

	VkPipelineColorBlendStateCreateInfo color_blend_state{
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable   = false,
		.attachmentCount = blend_states.size(),
		.pAttachments    = blend_states.data()
	};

	/* ====== Dynamic State ====== */
	auto dynamic_states = std::to_array({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = dynamic_states.size(),
		.pDynamicStates    = dynamic_states.data()
	};

	auto pipeline_result = Graphics_pipeline::create(
							   env.device,
							   shader_stages,
							   pipeline_layout,
							   render_target.renderpass,
							   vertex_input_state,
							   input_assembly_state,
							   viewport_state,
							   rasterization_state,
							   multisample_state,
							   depth_stencil_state,
							   color_blend_state,
							   dynamic_state,
							   std::nullopt
						   )
		>> handle;
	CHECK_RESULT(pipeline_result, "Create Pipeline for Deferred Rendering Failed");
}

void Deferred_pipeline::create_uniform_buffer(const App_environment& env, const App_swapchain& swapchain)
{
	uniform_buffer.clear();

	for (auto _ : Range(swapchain.image_count))
	{
		auto result = Buffer::create<Camera_uniform>(
						  env.allocator,
						  1,
						  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						  VMA_MEMORY_USAGE_CPU_TO_GPU
					  )
			>> uniform_buffer.emplace_back();
		CHECK_RESULT(result, "Create Uniform Buffers for Deferred Pipeline Failed");
	}
}

void Deferred_pipeline::create(
	const App_environment&        env,
	const App_swapchain&          swapchain,
	const Deferred_render_target& render_target
)
{
	create_uniform_buffer(env, swapchain);
	create_layouts(env, swapchain);
	create_descriptors(env, swapchain);
	create_pipeline(env, swapchain, render_target);
}

void Deferred_pipeline::recreate(
	const App_environment&        env,
	const App_swapchain&          swapchain,
	const Deferred_render_target& render_target
)
{
	create_descriptors(env, swapchain);
	create_pipeline(env, swapchain, render_target);
}