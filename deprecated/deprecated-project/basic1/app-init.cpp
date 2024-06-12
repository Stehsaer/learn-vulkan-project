#include "app.hpp"
#include "stb_image.h"

std::array<VkVertexInputAttributeDescription, 2> Vertex::get_vertex_attributes(uint32_t binding)
{
	VkVertexInputAttributeDescription attribute_pos_description{
		.location = 0,
		.binding  = binding,
		.format   = VK_FORMAT_R32G32B32_SFLOAT,
		.offset   = offsetof(Vertex, pos)
	};

	VkVertexInputAttributeDescription attribute_texcoord_description{
		.location = 1,
		.binding  = binding,
		.format   = VK_FORMAT_R32G32_SFLOAT,
		.offset   = offsetof(Vertex, texcoord)
	};

	return std::to_array({attribute_pos_description, attribute_texcoord_description});
}

void Application::init()
{
	init_window();
	init_instance();
	init_device();
	init_swapchain();
	init_command_buffer();
	init_texture();
	init_depth_buffer();
	init_pipeline();
	init_signals();
	init_buffers();
	init_descriptors();
}

void Application::init_window()
{
	bool window_success = Window::create(width, height, "Vulkan Test", nullptr, true) >> window;
	check_success(window_success, "Can't create window");
}

void Application::init_instance()
{
	/* STEPS
	1. Fill in Application Info
	2. Check Debug Utils Support
	3. Create Instance
	4. Create Debug Utils
	*/

	// 1. Fill in Application Info
	VkApplicationInfo app_info{
		.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext            = nullptr,
		.pApplicationName = "App",
		.apiVersion       = VK_API_VERSION_1_1
	};

	// 2. Check Debug Utils Support
	bool use_debug_utils =
#ifndef NDEBUG
		Debug_utility::is_supported()
#else
		false
#endif
		;

	auto extensions = glfw_extension_names();
	if (use_debug_utils)
	{
		extensions.push_back(Debug_utility::debug_utils_ext_name);
		layers.push_back(Debug_utility::validation_layer_name);
	}

	// 3. Create instance
	auto instance_result = Instance::create(app_info, extensions, layers) >> instance;
	check_success(instance_result, "Can't create instance");

	// 4. Create Debug Utils
	if (use_debug_utils)
	{
		auto debug_utils_result
			= Debug_utility::create(
				  instance,
				  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
				  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
					  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				  Debug_utility::default_callback
			  )
			>> debug_utils;
		check_success(debug_utils_result, "Can't create debug utility!");
	}
}

// initialize device
void Application::init_device()
{
	/* STEPS
	1. Enumerate Physical Devices
	2. Select Physical Devices
	3. Create Surface
	4. Find Graphics and Presentation Queue Families
	5. Queue create info & check Swapchain support
	6. Create Logical Device
	7. Get Queue Handles
	8. Create Memory Allocator
	*/

	// 1. Enumerate Physical Devices
	const auto physical_device_list = Physical_device::enumerate_from(instance);

	tools::print("Available Devices:");
	for (const auto& physical_device : physical_device_list)
	{
		tools::print("-- {}", physical_device.properties().deviceName);
	}

	// 2. Select Physical Devices
	const auto& selected_physical_device_iter = std::find_if(
		physical_device_list.begin(),
		physical_device_list.end(),
		[](const Physical_device& device) -> bool
		{
			return device.properties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
				|| device.properties().deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
		}
	);
	check_success(
		selected_physical_device_iter != physical_device_list.cend(),
		"Can't find a suitable graphics device!"
	);
	check_success((*selected_physical_device_iter).check_extensions_support({}), "Extension Not Supported!");
	physical_device = *selected_physical_device_iter;

	// 3. Create Surface
	auto surface_create_result = Surface::create(instance, window) >> surface;
	check_success(surface_create_result, "Can't create surface!");

	// 4. Find Queue Families
	auto g_family = physical_device.match_queue_family(
		[](VkQueueFamilyProperties property) -> bool
		{
			return property.queueCount > 0 && (property.queueFlags & VK_QUEUE_GRAPHICS_BIT);
		}
	);
	auto p_family = physical_device.match_present_queue_family(surface);
	check_success(g_family && p_family, "Can't find a graphics queue from physical device!");
	graphics_queue_idx     = g_family.value();
	presentation_queue_idx = p_family.value();

	// 5.1 Queue create info

	float queue_priority = 1.0f;

	VkDeviceQueueCreateInfo g_queue_create_info{
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphics_queue_idx,
		.queueCount       = 1,
		.pQueuePriorities = &queue_priority
	};

	VkDeviceQueueCreateInfo p_queue_create_info{
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = presentation_queue_idx,
		.queueCount       = 1,
		.pQueuePriorities = &queue_priority
	};

	// 5.2 Check Swapchain Support
	// - Extension Support
	// - Swapchain Support

	auto device_extensions = std::to_array({VK_KHR_SWAPCHAIN_EXTENSION_NAME});
	check_success(physical_device.check_extensions_support(device_extensions), "No Swapchain support!");

	check_success(
		[=, this]() -> bool
		{
			const auto detail = physical_device.get_swapchain_detail(surface);
			return !detail.formats.empty() && !detail.present_modes.empty();
		}(),
		"Swapchain not supported!"
	);

	// 6. Create Logical Device

	auto extensions  = std::to_array({VK_KHR_SWAPCHAIN_EXTENSION_NAME});
	auto queue_infos = std::to_array({g_queue_create_info, p_queue_create_info});

	tools::print("-- Check: Sampler Anistrophy = {}", physical_device.features().samplerAnisotropy);
	VkPhysicalDeviceFeatures requested_features = {};
	if (physical_device.features().samplerAnisotropy)
		requested_features.samplerAnisotropy = true;  // turn on anisotrophy

	auto device_create_result
		= Device::create(
			  instance,
			  physical_device,
			  std::span(queue_infos).subspan(0, graphics_queue_idx == presentation_queue_idx ? 1 : 2),
			  layers,
			  extensions,
			  requested_features
		  )
		>> main_device;
	check_success(device_create_result, "Can't create logical device!");

	// 7. Get Queue Handles
	graphics_queue     = main_device.get_queue_handle(graphics_queue_idx, 0);
	presentation_queue = graphics_queue_idx == presentation_queue_idx
		? graphics_queue
		: main_device.get_queue_handle(presentation_queue_idx, 0);

	// 8. Create Memory Allocator
	auto allocator_result = Vma_allocator::create(physical_device, main_device) >> allocator;
	check_success(allocator_result, "Failed to create allocator");
}

void Application::init_swapchain()
{
	/* STEPS
	1. Choose Surface Format
	2. Choose Swap Mode
	3. Calculate Swap Extent (Swap Resolution)
	4. Calculate Image Count
	5. Create Swapchain
	6. Acquire Images from Swapchain
	7. Create Image View
	*/

	// swapchain detail
	const auto swapchain_detail = physical_device.get_swapchain_detail(surface);

	// 1. Choose Surface Format
	auto choose_format = [=]() -> VkSurfaceFormatKHR
	{
		// No format
		if (swapchain_detail.formats.size() == 1 && swapchain_detail.formats[0].format == VK_FORMAT_UNDEFINED)
		{
			return {VK_FORMAT_R8G8B8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		}

		// Search for (R8G8B8-UNORM) and (SRGB_NONLINEAR)
		for (const auto& format : swapchain_detail.formats)
			if (format.format == VK_FORMAT_R8G8B8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;

		return swapchain_detail.formats[0];
	};

	swapchain_image_format = choose_format();

	// 2. Choose Swap Mode
	auto choose_swap_mode = [=]() -> VkPresentModeKHR
	{
		// First priority: triple-buffer
		for (const auto& mode : swapchain_detail.present_modes)
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;

		// Fallback: immediate
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	};

	present_mode = choose_swap_mode();

	// 3. Calculate Swap Extent / Resolution
	swap_extent = [=]() -> VkExtent2D
	{
		if (swapchain_detail.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return swapchain_detail.capabilities.currentExtent;
		}

		VkExtent2D actual_extent = {width, height};

		auto min = swapchain_detail.capabilities.minImageExtent, max = swapchain_detail.capabilities.maxImageExtent;

		uint32_t width  = std::clamp(actual_extent.width, min.width, max.width),
				 height = std::clamp(actual_extent.height, min.height, max.height);

		return {width, height};
	}();

	// 4. Calculate Image Count
	swapchain_image_count = swapchain_detail.capabilities.minImageCount + 1;
	if (swapchain_detail.capabilities.maxImageCount > 0
		&& swapchain_image_count > swapchain_detail.capabilities.maxImageCount)
		swapchain_image_count = swapchain_detail.capabilities.maxImageCount;

	// 5. Create Swapchain
	auto queue_indexes = std::to_array({graphics_queue_idx, presentation_queue_idx});

	auto swapchain_create_result
		= Swapchain::create(
			  main_device,
			  surface,
			  swapchain_image_format,
			  present_mode,
			  swap_extent,
			  swapchain_image_count,
			  std::span(queue_indexes).subspan(0, graphics_queue_idx == presentation_queue_idx ? 1 : 2),
			  swapchain_detail.capabilities.currentTransform
		  )
		>> swapchain;
	check_success(swapchain_create_result, "Can't create swapchain!");

	// 6. Acquire Images from Swapchain
	swapchain_images = swapchain.get_images();

	// 7. Create Image View
	swapchain_image_views.clear();
	swapchain_image_views.reserve(swapchain_images.size());
	for (const auto& image : swapchain_images)
	{
		auto image_view_result = Image_view::create(
									 main_device,
									 image,
									 swapchain_image_format.format,
									 utility_values::default_component_mapping,
									 VK_IMAGE_VIEW_TYPE_2D,
									 {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
								 )
			>> swapchain_image_views.emplace_back();
		check_success(image_view_result, "Can't create image view!");
	}
}

void Application::init_pipeline()
{
	// Create Shader Module
	// 1.1 Read files
	auto vert_binary_result = tools::read_binary("shaders/main.vert.spv");
	auto frag_binary_result = tools::read_binary("shaders/main.frag.spv");

	check_success(vert_binary_result, "Can't read vertex shader binary!");
	check_success(frag_binary_result, "Can't read fragment shader binary!");

	// 1.2 Create Shader Module
	auto vert_shader_result = Shader_module::create(main_device, vert_binary_result.val());
	auto frag_shader_result = Shader_module::create(main_device, frag_binary_result.val());

	check_success(vert_shader_result, "Can't create vertex shader module!");
	check_success(frag_shader_result, "Can't create fragment shader module!");

	auto shader_stages = std::to_array(
		{vert_shader_result.val().stage_info(VK_SHADER_STAGE_VERTEX_BIT),
		 frag_shader_result.val().stage_info(VK_SHADER_STAGE_FRAGMENT_BIT)}
	);

	// Configure Pipeline

	// Vertex Buffer Descriptions
	VkVertexInputBindingDescription binding0_description{
		.binding   = 0,
		.stride    = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	auto vertex_binding_descriptions   = std::to_array({binding0_description});
	auto vertex_attribute_descriptions = Vertex::get_vertex_attributes(0);

	// Vertex input
	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.flags                           = 0,
		.vertexBindingDescriptionCount   = (uint32_t)vertex_binding_descriptions.size(),
		.pVertexBindingDescriptions      = vertex_binding_descriptions.data(),
		.vertexAttributeDescriptionCount = (uint32_t)vertex_attribute_descriptions.size(),
		.pVertexAttributeDescriptions    = vertex_attribute_descriptions.data(),
	};

	// Input Assembly
	VkPipelineInputAssemblyStateCreateInfo input_state_create_info{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.flags                  = 0,
		.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = false
	};

	// Viewport and scissors
	VkViewport viewport{0, 0, (float)swap_extent.width, (float)swap_extent.height, 0.0, 1.0};
	tools::flip_viewport(viewport);

	VkRect2D scissor{
		.offset = {0,                 0                 },
		.extent = {swap_extent.width, swap_extent.height}
	};

	VkPipelineViewportStateCreateInfo viewport_state_create_info{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = &viewport,
		.scissorCount  = 1,
		.pScissors     = &scissor,
	};

	// Rasterization
	VkPipelineRasterizationStateCreateInfo raster_state_create_info{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.flags                   = 0,
		.depthClampEnable        = false,
		.rasterizerDiscardEnable = false,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.cullMode                = VK_CULL_MODE_NONE,
		.frontFace               = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth               = 1.0f
	};

	// Multisample
	VkPipelineMultisampleStateCreateInfo multisample_state_create_info{
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.flags                = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable       = true,
		.depthWriteEnable      = true,
		.depthCompareOp        = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = false,
		.stencilTestEnable     = false
	};

	auto color_blend_attachment0 = VkPipelineColorBlendAttachmentState{
		.blendEnable         = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp        = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp        = VK_BLEND_OP_ADD,
		.colorWriteMask
		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	// Color Blend for each attachment
	auto color_blend_attachment_list = std::to_array({color_blend_attachment0});

	// Color Blend State
	// auto color_blend_state = pipeline_helper::assemble_color_blend_info(
	// 	color_blend_attachment_list, false, VK_LOGIC_OP_COPY, {0.0f, 0.0f, 0.0f,
	// 0.0f}
	// );

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.flags           = 0,
		.logicOpEnable   = false,
		.attachmentCount = color_blend_attachment_list.size(),
		.pAttachments    = color_blend_attachment_list.data()
	};

	// Dynamic State
	auto dynamic_states = std::to_array({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

	VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.flags             = 0,
		.dynamicStateCount = (uint32_t)dynamic_states.size(),
		.pDynamicStates    = dynamic_states.data()
	};

	// Uniform buffer descriptor set
	VkDescriptorSetLayoutBinding ubo_binding{
		.binding            = 0,
		.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount    = 1,
		.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr
	};

	// Sampler descriptor set
	VkDescriptorSetLayoutBinding sampler_binding{
		.binding            = 1,
		.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount    = 1,
		.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};

	auto layout_bindings = std::to_array({ubo_binding, sampler_binding});

	auto descriptor_set_layout_result
		= Descriptor_set_layout::create(main_device, layout_bindings) >> descriptor_set_layout;
	check_success(descriptor_set_layout_result, "Can't create descriptor set layout!");

	auto descriptor_sets = std::to_array({*descriptor_set_layout});

	auto pipeline_layout_result = Pipeline_layout::create(main_device, descriptor_sets) >> pipeline_layout;
	check_success(pipeline_layout_result, "Can't create pipeline layout!");

	// Create Render Pass

	VkAttachmentDescription color_attachment_description{
		.flags          = 0,
		.format         = swapchain_image_format.format,
		.samples        = VK_SAMPLE_COUNT_1_BIT,
		.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentDescription depth_buffer_description{
		.flags          = 0,
		.format         = VK_FORMAT_D32_SFLOAT,
		.samples        = VK_SAMPLE_COUNT_1_BIT,
		.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference attachment0_ref{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depth_buffer_ref{.attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass0_description{
		.flags                   = 0,
		.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount    = 1,
		.pColorAttachments       = &attachment0_ref,
		.pDepthStencilAttachment = &depth_buffer_ref
	};

	VkSubpassDependency subpass0_dependency{
		.srcSubpass    = VK_SUBPASS_EXTERNAL,
		.dstSubpass    = 0,
		.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};

	auto attachment_descriptions = std::to_array({color_attachment_description, depth_buffer_description});
	auto subpass_descriptions    = std::to_array({subpass0_description});
	auto subpass_dependencies    = std::to_array({subpass0_dependency});

	auto render_pass_result
		= Render_pass::create(main_device, attachment_descriptions, subpass_descriptions, subpass_dependencies)
		>> render_pass;
	check_success(render_pass_result, "Can't create render pass!");

	// Create Graphics Pipeline

	auto graphic_pipeline_result = Graphics_pipeline::create(
									   main_device,
									   shader_stages,
									   pipeline_layout,
									   render_pass,
									   vertex_input_state_create_info,
									   input_state_create_info,
									   viewport_state_create_info,
									   raster_state_create_info,
									   multisample_state_create_info,
									   depth_stencil_state_create_info,
									   color_blend_state_create_info,
									   dynamic_state_create_info,
									   std::nullopt
								   )
		>> graphics_pipeline;
	check_success(graphic_pipeline_result, "Can't create graphics pipeline!");

	// Create Framebuffer
	framebuffers.resize(swapchain_image_views.size());

	for (uint32_t i = 0; i < swapchain_image_views.size(); i++)
	{
		auto attachments = std::to_array({*swapchain_image_views[i], *depth_buffer_view});

		auto framebuffer_result
			= Framebuffer::create(main_device, render_pass, attachments, swap_extent.width, swap_extent.height)
			>> framebuffers[i];

		check_success(framebuffer_result, "Can't create framebuffer {}", i);
	}
}

void Application::init_command_buffer()
{
	/* STEPS
	1. Create Command Pool
	2. Create Command Buffer for each framebuffer
	*/

	// 1. Create Command Pool
	auto cmd_pool_result
		= Command_pool::create(main_device, graphics_queue_idx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
		>> command_pool;
	check_success(cmd_pool_result, "Can't create command pool!");

	// 2. Create Command Buffers
	auto cmd_buffers_result
		= Command_buffer::allocate_from(command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> command_buffer;
	check_success(cmd_buffers_result, "Can't allocate command buffers!");
}

void Application::init_signals()
{
	check_success(Semaphore::create(main_device) >> image_available_semaphore, "Failed to create Semaphore");

	check_success(Semaphore::create(main_device) >> render_complete_semaphore, "Failed to create Semaphore");

	check_success(Fence::create(main_device, true) >> next_frame_fence, "Failed to create Fence");
}

void Application::init_buffers()
{
	// Create Vertex Buffer & Index buffer
	auto vertex_buffer_result = Buffer::create<Vertex>(
									allocator,
									vertex_list.size(),
									VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
									VMA_MEMORY_USAGE_GPU_ONLY
								)
		>> vertex_buffer;
	auto index_buffer_result = Buffer::create<Vertex>(
								   allocator,
								   index_list.size(),
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
								   VMA_MEMORY_USAGE_GPU_ONLY
							   )
		>> index_buffer;
	check_success(vertex_buffer_result, "Can't create vertex buffer!");
	check_success(index_buffer_result, "Can't create index buffer!");

	// Staging buffers
	Buffer vertex_staging_buffer;  // Temporary staging buffer
	auto   vertex_staging_buffer_result = Buffer::create<Vertex>(
                                            allocator,
                                            vertex_list.size(),
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_TO_GPU
                                        )
		>> vertex_staging_buffer;

	Buffer index_staging_buffer;
	auto   index_staging_buffer_result = Buffer::create<uint16_t>(
                                           allocator,
                                           index_list.size(),
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VMA_MEMORY_USAGE_CPU_TO_GPU
                                       )
		>> index_staging_buffer;

	check_success(vertex_staging_buffer_result, "Can't create stage buffer!");
	check_success(index_staging_buffer_result, "Can't create stage buffer!");

	// Transfer data
	check_success(
		vertex_staging_buffer->allocation.transfer(vertex_list),
		"Can't transfer vertex data to staging buffer!"
	);
	check_success(
		index_staging_buffer->allocation.transfer(index_list),
		"Can't transfer index data to staging buffer!"
	);

	// Record command buffer
	command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	command_buffer.cmd.copy_buffer(vertex_buffer, vertex_staging_buffer);
	command_buffer.cmd.copy_buffer(index_buffer, index_staging_buffer);
	check_success(command_buffer.end(), "Failed to record transfer command buffer");

	// Submit to queue
	auto         submit_command_buffers = std::to_array({*command_buffer});
	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};

	submit_info.pCommandBuffers    = submit_command_buffers.data();
	submit_info.commandBufferCount = submit_command_buffers.size();

	auto submit_result = vkQueueSubmit(graphics_queue, 1, &submit_info, nullptr);
	check_success(submit_result, "Can't submit queue!");
	vkQueueWaitIdle(graphics_queue);

	// Uniform Buffers
	for (uint32_t i = 0; i < swapchain_image_count; i++)
	{
		auto result = Buffer::create<Uniform_object>(
						  allocator,
						  1,
						  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						  VMA_MEMORY_USAGE_CPU_TO_GPU
					  )
			>> uniform_buffers.emplace_back();
		check_success(result, "Can't create uniform buffer!");
	}
}

void Application::init_texture()
{
	// 1. Read texture data
	int image_x, image_y, channels;

	std::unique_ptr<unsigned char, std::function<void(unsigned char*)>> image_data(
		stbi_load("uv.png", &image_x, &image_y, &channels, STBI_rgb_alpha),
		stbi_image_free
	);

	check_success(image_data != nullptr, "Failed to load image: {}", stbi_failure_reason());
	tools::print("-- Image loaded: {}x{}px", image_x, image_y);

	// 2. Create Staging Buffer and transfer image data to the staging buffer
	auto staging_buffer = Buffer::create<unsigned char>(
		allocator,
		image_x * image_y * 4,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);
	staging_buffer.val()->allocation.transfer(image_data.get(), image_x * image_y * 4);

	// 3. Create Image on GPU
	auto image_create_result = Image::create(
		allocator,
		VK_IMAGE_TYPE_2D,
		VkExtent3D(image_x, image_y, 1),
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		VK_SHARING_MODE_EXCLUSIVE
	);
	image_create_result >> texture;
	check_success(image_create_result, "Failed to create image");

	// 4. Layout Transformation & Data copy
	command_buffer.begin_single_submit();

	// transit to transfer-dst-optimal
	command_buffer.cmd.image_layout_transit(
		texture,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	);
	// copy buffer
	{
		VkBufferImageCopy copy_info;
		copy_info.bufferOffset = copy_info.bufferImageHeight = copy_info.bufferRowLength = 0;
		copy_info.imageExtent                     = VkExtent3D(image_x, image_y, 1);
		copy_info.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_info.imageSubresource.baseArrayLayer = 0;
		copy_info.imageSubresource.layerCount     = 1;
		copy_info.imageSubresource.mipLevel       = 0;
		copy_info.imageOffset                     = {0, 0, 0};

		command_buffer.cmd
			.copy_image(texture, staging_buffer.val(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, std::span(&copy_info, 1));
	}

	// transit to shader-readonly-optimal
	command_buffer.cmd.image_layout_transit(
		texture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VkImageSubresourceRange{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	);

	check_success(command_buffer.end(), "Can't end image layout transition commands!");

	// submit and wait until idle
	std::array<VkCommandBuffer, 1> cmd_buffers = {*command_buffer};
	VkSubmitInfo                   submit_info{
						  .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
						  .commandBufferCount = 1,
						  .pCommandBuffers    = cmd_buffers.data()
    };
	check_success(
		vkQueueSubmit(graphics_queue, 1, &submit_info, nullptr),
		"Can't submit image layout transition commands!"
	);
	vkQueueWaitIdle(graphics_queue);

	// 5. create image view
	auto texture_image_view_result = Image_view::create(
		main_device,
		texture,
		VK_FORMAT_R8G8B8A8_SRGB,
		utility_values::default_component_mapping,
		VK_IMAGE_VIEW_TYPE_2D,
		VkImageSubresourceRange{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	);
	texture_image_view_result >> texture_image_view;
	check_success(texture_image_view_result, "Can't create image view for texture");

	// 6. create sampler

	VkSamplerCreateInfo sampler_create_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_create_info.addressModeU = sampler_create_info.addressModeV = sampler_create_info.addressModeW
		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.minFilter               = VK_FILTER_LINEAR;
	sampler_create_info.magFilter               = VK_FILTER_LINEAR;
	sampler_create_info.anisotropyEnable        = physical_device.features().samplerAnisotropy;  // temporary disabled
	sampler_create_info.maxAnisotropy           = physical_device.properties().limits.maxSamplerAnisotropy;
	sampler_create_info.unnormalizedCoordinates = false;
	sampler_create_info.compareEnable           = false;
	sampler_create_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.mipLodBias              = 0.0f;
	sampler_create_info.maxLod                  = 0.0f;
	sampler_create_info.minLod                  = 0.0f;

	auto sampler_result = Image_sampler::create(main_device, sampler_create_info) >> sampler;
	check_success(sampler_result, "Can't create image sampler!");
}

void Application::init_descriptors()
{
	// 1. Generate pool

	VkDescriptorPoolSize uniform_buffer_descriptor_pool_size{
		.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = swapchain_image_count
	};

	VkDescriptorPoolSize sampler_pool_size{
		.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = swapchain_image_count
	};

	auto pool_sizes  = std::to_array({uniform_buffer_descriptor_pool_size, sampler_pool_size});
	auto pool_result = Descriptor_pool::create(main_device, pool_sizes, swapchain_image_count);

	pool_result >> descriptor_pool;
	check_success(pool_result, "Can't create descriptor pool!");

	std::vector<VkDescriptorSetLayout> set_layouts(swapchain_image_count, *descriptor_set_layout);

	auto descriptor_set_alloc_result
		= Descriptor_set::allocate_from(descriptor_pool, swapchain_image_count, set_layouts);
	descriptor_set_alloc_result >> descriptor_sets;
	check_success(descriptor_set_alloc_result, "Can't allocate descriptor sets!");
}

void Application::init_depth_buffer()
{
	// Create depth buffer
	auto depth_buffer_result = Image::create(
								   allocator,
								   VK_IMAGE_TYPE_2D,
								   {swap_extent.width, swap_extent.height, 1},
								   VK_FORMAT_D32_SFLOAT,
								   VK_IMAGE_TILING_OPTIMAL,
								   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
								   VMA_MEMORY_USAGE_GPU_ONLY,
								   VK_SHARING_MODE_EXCLUSIVE
							   )
		>> depth_buffer;
	check_success(depth_buffer_result, "Can't create depth buffer");

	auto depth_buffer_view_result = Image_view::create(
										main_device,
										depth_buffer,
										VK_FORMAT_D32_SFLOAT,
										utility_values::default_component_mapping,
										VK_IMAGE_VIEW_TYPE_2D,
										VkImageSubresourceRange{
											.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
											.baseMipLevel   = 0,
											.levelCount     = 1,
											.baseArrayLayer = 0,
											.layerCount     = 1
										}
									)
		>> depth_buffer_view;

	command_buffer.begin_single_submit();

	// depth buffer transition
	command_buffer.cmd.image_layout_transit(
		depth_buffer,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VkImageSubresourceRange{
			.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	);
	check_success(command_buffer.end(), "Can't end command buffer");

	VkCommandBuffer buf = *command_buffer;
	VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &buf};
	vkQueueSubmit(graphics_queue, 1, &submit_info, nullptr);
	vkQueueWaitIdle(graphics_queue);
}