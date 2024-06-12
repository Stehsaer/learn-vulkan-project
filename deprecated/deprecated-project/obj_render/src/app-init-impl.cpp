#include "application.hpp"
#include "stb_image.h"
#include "tiny_obj_loader.h"
#include <chrono>
#include <set>

#define PRINT_SWAPCHAIN_FORMAT true

std::array<VkVertexInputAttributeDescription, 3> Vertex_object::get_vertex_attributes(uint32_t binding)
{
	VkVertexInputAttributeDescription pos_description{
		.location = 0,
		.binding  = binding,
		.format   = VK_FORMAT_R32G32B32_SFLOAT,
		.offset   = offsetof(Vertex_object, pos)
	},
		normal_description{
			.location = 1,
			.binding  = binding,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = offsetof(Vertex_object, normal)
		},
		uv_description{
			.location = 2,
			.binding  = binding,
			.format   = VK_FORMAT_R32G32_SFLOAT,
			.offset   = offsetof(Vertex_object, uv)
		};

	return std::to_array({pos_description, normal_description, uv_description});
}

void Obj_app::init(const std::string& model_path, const std::string& img_path)
{
	environment.init(800, 600, "Wavefront Obj Renderer");
	swapchain.init(environment);
	renderer.init(environment, swapchain, model_path, img_path);
	sync.init(environment);
	imgui_render.init(environment, swapchain);
}

void App_environment::init(int init_width, int init_height, const char* title_name)
{
	// Assertions
	if (init_width <= 0 || init_height <= 0)
		throw std::runtime_error("Incorrect argument: init_width and init_height must be positive");

	// Extensions and layers
	std::vector<const char*> instance_extensions = glfw_extension_names();
	std::vector<const char*> layers              = {};

	// Create window
	{
		auto result = Window::create(init_width, init_height, title_name, nullptr, true, true) >> window;
		if (!result) throw std::runtime_error("Create window failed: " + result.err());
	}

	// Create instance & debug utility
	{
		bool enable_debug_utility = false;

#ifndef NDEBUG
		// Debug utility enabled
		if (Debug_utility::is_supported())
		{
			enable_debug_utility = true;
			instance_extensions.push_back(Debug_utility::debug_utils_ext_name);
			layers.push_back(Debug_utility::validation_layer_name);
		}
#endif

		VkApplicationInfo app_info{
			.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Object Renderer",
			.apiVersion       = VK_API_VERSION_1_1
		};

		// Create instance
		auto result = Instance::create(app_info, instance_extensions, layers) >> instance;
		CHECK_RESULT(result, "Create Instance");

		// Create Debug utility
		if (enable_debug_utility)
		{
			auto debug_utility_result
				= Debug_utility::create(
					  instance,
					  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
					  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
						  | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
						  | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
					  Debug_utility::default_callback
				  )
				>> debug_utility;
		}
	}

	// Create window surface
	{
		auto result = Surface::create(instance, window) >> surface;
		CHECK_RESULT(result, "Create surface");
	}

	// Find physical device
	{
		// Enumerate available physical devices
		const auto physical_devices = Physical_device::enumerate_from(instance);

		// Select physical device
		Physical_device selected_physical_device;
		for (const auto& phy_device : physical_devices)  // search discrete gpu
			if (phy_device.properties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selected_physical_device = phy_device;
				break;
			}
		if (selected_physical_device.is_empty())  // search intergrated gpu
			for (const auto& phy_device : physical_devices)
				if (phy_device.properties().deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
				{
					selected_physical_device = phy_device;
				}
		if (selected_physical_device.is_empty()) throw std::runtime_error("No suitable physical device");

		physical_device = selected_physical_device;
	}

	// Create logical device
	{
		// match graphics queue family
		auto g_family_match = physical_device.match_queue_family(
			[](VkQueueFamilyProperties property) -> bool
			{
				return property.queueCount > 0 && (property.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					&& (property.queueFlags & VK_QUEUE_TRANSFER_BIT);
			}
		);

		// match present queue family
		auto p_family_match = physical_device.match_present_queue_family(surface);

		if (!g_family_match || !p_family_match) throw std::runtime_error("Match queue families failed");

		g_queue_family = g_family_match.value();
		p_queue_family = p_family_match.value();

		// queue create infos
		float queue_priority = 1.0f;

		VkDeviceQueueCreateInfo g_queue_create_info{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = g_queue_family,
			.queueCount       = 1,
			.pQueuePriorities = &queue_priority
		};

		VkDeviceQueueCreateInfo p_queue_create_info{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = p_queue_family,
			.queueCount       = 1,
			.pQueuePriorities = &queue_priority
		};

		auto queue_create_infos     = std::to_array({g_queue_create_info, p_queue_create_info});
		auto queue_create_info_span = std::span(queue_create_infos.begin(), g_queue_family == p_queue_family ? 1 : 2);

		// device extensions
		auto device_extensions   = std::to_array({VK_KHR_SWAPCHAIN_EXTENSION_NAME});
		bool extension_available = physical_device.check_extensions_support(device_extensions);
		if (!extension_available) throw std::runtime_error("Device extensions unavailable");

		// device features (anisotrophy)
		VkPhysicalDeviceFeatures requested_features{};
		if (physical_device.features().samplerAnisotropy) requested_features.samplerAnisotropy = true;

		// Create logical device
		auto result = Device::create(
						  instance,
						  physical_device,
						  queue_create_info_span,
						  layers,
						  device_extensions,
						  requested_features
					  )
			>> device;
		CHECK_RESULT(result, "Create device");
	}

	// Acquire queue handles
	{
		g_queue = device.get_queue_handle(g_queue_family, 0);
		p_queue = device.get_queue_handle(p_queue_family, 0);
	}

	// Create VMA allocator
	{
		auto result = Vma_allocator::create(physical_device, device) >> allocator;
		CHECK_RESULT(result, "Create VMA Allocator");
	}

	// Create command pool
	{
		auto result = Command_pool::create(device, g_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
			>> command_pool;
		CHECK_RESULT(result, "Create Command Pool");
	}
}

VkSurfaceFormatKHR App_swapchain::select_swapchain_format(const Swapchain_detail& swapchain_detail)
{
	// No format
	if (swapchain_detail.formats.size() == 1 && swapchain_detail.formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_R8G8B8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	// Select format from video mode
	VkFormat target_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

	// Search for target format
	for (const auto& format : swapchain_detail.formats)
		if (format.format == target_format && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;

	// fallback: search for RGB8
	for (const auto& format : swapchain_detail.formats)
		if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;

	return swapchain_detail.formats[0];
}

void App_swapchain::init(const App_environment& env)
{
	// Create swapchain
	{
		auto swapchain_detail = env.physical_device.get_swapchain_detail(env.surface);

		// choose swapchain format
		format = select_swapchain_format(swapchain_detail);

		// choose present mode
		VkPresentModeKHR present_mode = [=]() -> VkPresentModeKHR
		{
			// first priority: triple buffer
			for (const auto& mode : swapchain_detail.present_modes)
				if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;

			// fallback: immediate
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
		}();

		// get swap extent
		int width, height;
		glfwGetFramebufferSize(*env.window, &width, &height);
		extent.width = width, extent.height = height;

		// get swap image count
		image_count = swapchain_detail.capabilities.minImageCount + 1;
		if (swapchain_detail.capabilities.maxImageCount > 0)
			image_count = std::min(image_count, swapchain_detail.capabilities.maxImageCount);

		// queue families
		auto queue_families    = std::to_array({env.g_queue_family, env.p_queue_family});
		auto queue_family_span = std::span(queue_families.begin(), env.g_queue_family == env.p_queue_family ? 1 : 2);

		// create swapchain
		auto result = Swapchain::create(
						  env.device,
						  env.surface,
						  format,
						  present_mode,
						  extent,
						  image_count,
						  queue_family_span,
						  swapchain_detail.capabilities.currentTransform,
						  *handle
					  )
			>> handle;
		CHECK_RESULT(result, "Create Swapchain");
	}

	// Get swapchain images & image views
	{
		// Image
		images = handle.get_images();
		image_views.clear();  // clear original

		// Image views
		for (auto i : Range(image_count))
		{
			auto result = Image_view::create(
							  env.device,
							  images[i],
							  format.format,
							  utility_values::default_component_mapping,
							  VK_IMAGE_VIEW_TYPE_2D,
							  VkImageSubresourceRange{
								  .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
								  .baseMipLevel   = 0,
								  .levelCount     = 1,
								  .baseArrayLayer = 0,
								  .layerCount     = 1
							  }
						  )
				>> image_views.emplace_back();
			CHECK_RESULT(result, "Create Swapchain Image Views");
		}
	}

	// Create Depth Buffer
	{
		// create & allocate image for depth buffer
		auto depth_buffer_result = Image::create(
									   env.allocator,
									   VK_IMAGE_TYPE_2D,
									   {extent.width, extent.height, 1},
									   VK_FORMAT_D32_SFLOAT,
									   VK_IMAGE_TILING_OPTIMAL,
									   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
									   VMA_MEMORY_USAGE_GPU_ONLY,
									   VK_SHARING_MODE_EXCLUSIVE
								   )
			>> depth_buffer;
		CHECK_RESULT(depth_buffer_result, "Create Depth Buffer");

		// create image view for depth buffer
		auto depth_buffer_view_result = Image_view::create(
											env.device,
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
		CHECK_RESULT(depth_buffer_view_result, "Create Depth Buffer View");
	}
}

void App_render::init(
	const App_environment& env,
	const App_swapchain&   swapchain,
	const std::string&     model_path,
	const std::string&     img_path
)
{
	auto result = Command_buffer::allocate_from(env.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> command_buf;
	CHECK_RESULT(result, "Create Command Buffer");

	pipeline.init(env, swapchain);
	read_image(env, img_path);
	read_model(env, model_path);
	create_uniform(env, swapchain);
}

void App_pipeline::init(const App_environment& env, const App_swapchain& swapchain, bool recreate)
{
	// Create descriptor set layout
	if (!recreate)
	{
		VkDescriptorSetLayoutBinding ubo_binding{
			.binding         = 0,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT
		};

		VkDescriptorSetLayoutBinding texture_binding{
			.binding         = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
		};

		auto bindings = std::to_array({ubo_binding, texture_binding});

		auto result = Descriptor_set_layout::create(env.device, bindings) >> descriptor_set_layout;
		CHECK_RESULT(result, "Create Descriptor Set Layout");
	}

	// Create descriptors
	create_descriptors(env, swapchain);

	// Create pipeline layout
	if (!recreate)
	{
		auto descriptor_set_layouts = std::to_array({*descriptor_set_layout});

		auto result = Pipeline_layout::create(env.device, descriptor_set_layouts) >> layout;
		CHECK_RESULT(result, "Create Pipeline Layout");
	}

	// Create render pass
	if (!recreate)
	{
		VkAttachmentDescription color_attachment_description{
			.format        = swapchain.format.format,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkAttachmentDescription depth_attachment_description{
			.format        = VK_FORMAT_D32_SFLOAT,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};

		VkAttachmentReference color_attachment_ref{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			depth_attachment_ref{.attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkSubpassDescription subpass_description{
			.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount    = 1,
			.pColorAttachments       = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref
		};

		// - color output: color attachment;
		// - fragment tests: depth buffer
		VkSubpassDependency subpass_dependency{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0,
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		};

		auto attachment_descriptions = std::to_array({color_attachment_description, depth_attachment_description});
		auto subpass_descriptions    = std::to_array({subpass_description});
		auto subpass_dependencies    = std::to_array({subpass_dependency});

		auto result
			= Render_pass::create(env.device, attachment_descriptions, subpass_descriptions, subpass_dependencies)
			>> render_pass;
		CHECK_RESULT(result, "Create Render Pass");
	}

	// Create Graphics Pipeline
	if (!recreate)
	{
		// Shaders
		auto vertex_shader_binary   = tools::read_binary("shaders/main.vert.spv"),
			 fragment_shader_binary = tools::read_binary("shaders/main.frag.spv");

		if (!vertex_shader_binary || !fragment_shader_binary) throw std::runtime_error("Read shader binaries");

		auto vertex_shader   = Shader_module::create(env.device, vertex_shader_binary.val()),
			 fragment_shader = Shader_module::create(env.device, fragment_shader_binary.val());

		CHECK_RESULT(vertex_shader, "Create Vertex Shader Module");
		CHECK_RESULT(fragment_shader, "Create Fragment Shader Module");

		auto shader_stage_infos = std::to_array(
			{vertex_shader.val().stage_info(VK_SHADER_STAGE_VERTEX_BIT),
			 fragment_shader.val().stage_info(VK_SHADER_STAGE_FRAGMENT_BIT)}
		);

		// Vertex Input
		auto vertex_input_attribute_descriptions = Vertex_object::get_vertex_attributes(0);

		VkVertexInputBindingDescription input_binding_description{
			.binding   = 0,
			.stride    = sizeof(Vertex_object),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};

		VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{
			.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount   = 1,
			.pVertexBindingDescriptions      = &input_binding_description,
			.vertexAttributeDescriptionCount = (uint32_t)vertex_input_attribute_descriptions.size(),
			.pVertexAttributeDescriptions    = vertex_input_attribute_descriptions.data()
		};

		// Input Assembly
		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info{
			.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};

		// Viewport State
		VkViewport viewport{
			.x        = 0,
			.y        = 0,
			.width    = (float)swapchain.extent.width,
			.height   = (float)swapchain.extent.height,
			.minDepth = 0,
			.maxDepth = 1
		};
		tools::flip_viewport(viewport);

		VkRect2D scissor{
			.offset = {0, 0},
			.extent = swapchain.extent
		};

		VkPipelineViewportStateCreateInfo viewport_state_info{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports    = &viewport,
			.scissorCount  = 1,
			.pScissors     = &scissor
		};

		// Rasterization
		VkPipelineRasterizationStateCreateInfo rasterization_state_info{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable        = false,
			.rasterizerDiscardEnable = false,
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = VK_CULL_MODE_BACK_BIT,
			.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable         = false,
			.lineWidth               = 1
		};

		auto attachment_blend_state = std::to_array({utility_values::default_color_blend_state});

		VkPipelineColorBlendStateCreateInfo color_blend_info{
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = (uint32_t)attachment_blend_state.size(),
			.pAttachments    = attachment_blend_state.data()
		};

		// Dynamic states
		auto dynamic_states = std::to_array({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

		VkPipelineDynamicStateCreateInfo dynamic_states_info{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = (uint32_t)dynamic_states.size(),
			.pDynamicStates    = dynamic_states.data()
		};

		auto result = Graphics_pipeline::create(
						  env.device,
						  shader_stage_infos,
						  layout,
						  render_pass,
						  vertex_input_state_create_info,
						  input_assembly_state_info,
						  viewport_state_info,
						  rasterization_state_info,
						  utility_values::multisample_state_disabled,
						  utility_values::default_depth_test_enabled,
						  color_blend_info,
						  dynamic_states_info,
						  {}
					  )
			>> handle;
		CHECK_RESULT(result, "Create Graphics Pipeline");
	}

	// Create Frame Buffer
	{
		framebuffers.clear();

		for (uint32_t i = 0; i < swapchain.image_count; i++)
		{
			auto attachments = std::to_array({*swapchain.image_views[i], *swapchain.depth_buffer_view});
			auto result      = Framebuffer::create(
                              env.device,
                              render_pass,
                              attachments,
                              swapchain.extent.width,
                              swapchain.extent.height
                          )
				>> framebuffers.emplace_back();
			CHECK_RESULT(result, "Crate Framebuffers");
		}
	}
}

void App_imgui_render::init(const App_environment& env, const App_swapchain& swapchain)
{
	init_graphics(env, swapchain);
	init_imgui(env, swapchain);
}

void App_imgui_render::init_imgui(const App_environment& env, const App_swapchain& swapchain)
{
	// Allocate descriptor pool
	{
		auto pool_sizes = std::to_array<VkDescriptorPoolSize>({
			{VK_DESCRIPTOR_TYPE_SAMPLER,                1000},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000},
			{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000}
		});

		auto result
			= Descriptor_pool::create(env.device, pool_sizes, 1000, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
			>> descriptor_pool;
		CHECK_RESULT(result, "Create Descriptor Pool For IMGUI");
	}
	// Init imgui context
	{
		ImGui::CreateContext();

		// IO & Style
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		ImGui::StyleColorsDark();

		// Scale
		float x_scale, y_scale;
		glfwGetWindowContentScale(*env.window, &x_scale, &y_scale);
		float scale = std::max(x_scale, y_scale);

		// Fonts
		ImFontConfig cfg;
		if (scale > 1) cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_Bold;
		cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_ForceAutoHint;

		io.Fonts->AddFontFromFileTTF("fonts/cantarell.otf", 16 * scale, &cfg);
		ImGui::GetStyle().ScaleAllSizes(scale);

		ImGui_ImplGlfw_InitForVulkan(*env.window, true);

		ImGui_ImplVulkan_InitInfo imgui_vulkan_init_info{
			.Instance            = *env.instance,
			.PhysicalDevice      = *env.physical_device,
			.Device              = *env.device,
			.QueueFamily         = env.g_queue_family,
			.Queue               = env.g_queue,
			.DescriptorPool      = *descriptor_pool,
			.Subpass             = 0,
			.MinImageCount       = swapchain.image_count,
			.ImageCount          = swapchain.image_count,
			.MSAASamples         = VK_SAMPLE_COUNT_1_BIT,
			.UseDynamicRendering = false,
			.Allocator           = env.instance.allocator_ptr(),
			.CheckVkResultFn     = nullptr,
			.MinAllocationSize   = 0
		};

		ImGui_ImplVulkan_Init(&imgui_vulkan_init_info, *render_pass);
	}

	Command_buffer command_buf;

	// Allocate command buffer
	{
		auto result = Command_buffer::allocate_from(env.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> command_buf;
		CHECK_RESULT(result, "Create Command Buffer For IMGUI");
	}

	// Create font atlas
	{
		command_buf.begin_single_submit();
		ImGui_ImplVulkan_CreateFontsTexture();
		auto result = command_buf.end();
		if (result != VK_SUCCESS) throw std::runtime_error("End CmdBuf for IMGUI Fonts Failed");

		auto         command_buf_list = std::to_array({*command_buf});
		VkSubmitInfo info{
			.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers    = command_buf_list.data()
		};

		auto submit_result = vkQueueSubmit(env.g_queue, 1, &info, nullptr);
		if (submit_result != VK_SUCCESS) throw std::runtime_error("Submit CmdBuf for IMGUI Fonts Failed");
		vkQueueWaitIdle(env.g_queue);
	}
}

void App_imgui_render::init_graphics(const App_environment& env, const App_swapchain& swapchain)
{
	// Create renderpass
	{
		VkAttachmentDescription color_attachment_description{
			.format        = swapchain.format.format,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		VkAttachmentReference subpass_attachment_ref{
			.attachment = 0,
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpass_description{
			.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &subpass_attachment_ref
		};

		VkSubpassDependency subpass_dependency{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0,
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		};

		auto attachment_descriptions = std::to_array({color_attachment_description});
		auto subpass_descriptions    = std::to_array({subpass_description});
		auto subpass_dependencies    = std::to_array({subpass_dependency});

		auto result
			= Render_pass::create(env.device, attachment_descriptions, subpass_descriptions, subpass_dependencies)
			>> render_pass;
		CHECK_RESULT(result, "Create Render Pass for IMGUI");
	}

	// Create framebuffer
	{
		framebuffers.clear();
		for (uint32_t i = 0; i < swapchain.image_count; i++)
		{
			auto views = std::to_array({*swapchain.image_views[i]});
			auto result
				= Framebuffer::create(env.device, render_pass, views, swapchain.extent.width, swapchain.extent.height)
				>> framebuffers.emplace_back();
			CHECK_RESULT(result, "Create Framebuffer for IMGUI");
		}
	}
}

void App_imgui_render::new_frame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void App_render::read_image(const App_environment& env, const std::string& img_path)
{
	const int                mip_levels = 5;
	int                      img_width, img_height, img_channels;
	std::shared_ptr<uint8_t> img_data;
	Buffer                   image_staging_buffer;

	// Read image data
	{
		uint8_t* data = stbi_load(img_path.c_str(), &img_width, &img_height, &img_channels, STBI_rgb_alpha);
		if (data == nullptr) throw std::runtime_error(std::string("Read image:") + stbi_failure_reason());
		img_data = std::shared_ptr<uint8_t>(data, stbi_image_free);
	}

	// Create staging buffer & transfer data
	{
		auto result = Buffer::create<uint8_t>(
						  env.allocator,
						  img_width * img_height * STBI_rgb_alpha,
						  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						  VMA_MEMORY_USAGE_CPU_TO_GPU
					  )
			>> image_staging_buffer;
		CHECK_RESULT(result, "Create Image Staging Buffer");

		image_staging_buffer->allocation.transfer(img_data.get(), img_width * img_height * STBI_rgb_alpha);
	}

	// Create image
	{
		auto result
			= Image::create(
				  env.allocator,
				  VK_IMAGE_TYPE_2D,
				  VkExtent3D(img_width, img_height, 1),
				  VK_FORMAT_R8G8B8A8_UNORM,
				  VK_IMAGE_TILING_OPTIMAL,
				  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				  VMA_MEMORY_USAGE_GPU_ONLY,
				  VK_SHARING_MODE_EXCLUSIVE,
				  mip_levels
			  )
			>> obj_texture;
		CHECK_RESULT(result, "Create texture");
	}

	// Upload & Generate mipmap
	{
		Command_buffer cmd_buf;

		auto command_buffer_alloc_result
			= Command_buffer::allocate_from(env.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> cmd_buf;
		CHECK_RESULT(command_buffer_alloc_result, "Create Command Buffer");

		cmd_buf.begin_single_submit();
		cmd_buf.cmd.image_layout_transit(
			obj_texture,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = mip_levels,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		);

		VkBufferImageCopy copy_region;
		copy_region.bufferImageHeight = copy_region.bufferOffset = copy_region.bufferRowLength = 0;
		copy_region.imageSubresource = VkImageSubresourceLayers{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 1
		};
		copy_region.imageExtent.width  = img_width;
		copy_region.imageExtent.height = img_height;
		copy_region.imageExtent.depth  = 1;
		copy_region.imageOffset        = {0, 0, 0};

		cmd_buf.cmd
			.copy_image(obj_texture, image_staging_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {&copy_region, 1});

		int mip_width = img_width, mip_height = img_height;

		for (auto i : Range(1u, mip_levels))
		{
			// transit previous layer to TRANSFER_SRC
			cmd_buf.cmd.image_layout_transit(
				obj_texture,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				{VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1}
			);

			// Blit range
			VkImageBlit blit{
				.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1},
				.srcOffsets     = {{0, 0, 0}, {mip_width, mip_height, 1}},
				.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1},
				.dstOffsets = {{0, 0, 0}, {mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1}}
			};

			// BLIT!!!
			cmd_buf.cmd.blit_image(
				obj_texture,
				obj_texture,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_FILTER_LINEAR,
				{&blit, 1}
			);

			// Transit back to TRANSFER DST
			cmd_buf.cmd.image_layout_transit(
				obj_texture,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				{VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1}
			);

			if (mip_width > 1) mip_width /= 2;
			if (mip_height > 1) mip_height /= 2;
		}

		// Transit all to SHADER READ ONLY
		cmd_buf.cmd.image_layout_transit(
			obj_texture,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VkImageSubresourceRange{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = mip_levels,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		);

		auto end_result = cmd_buf.end();
		if (end_result != VK_SUCCESS)
			throw std::runtime_error(
				std::format("End image transfer command buffer: {}", magic_enum::enum_name(end_result))
			);

		std::array<VkCommandBuffer, 1> cmd_buffers = {*cmd_buf};

		VkSubmitInfo submit_info{
			.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers    = cmd_buffers.data()
		};

		vkQueueSubmit(env.g_queue, 1, &submit_info, nullptr);
		vkQueueWaitIdle(env.g_queue);
	}

	// Image View
	{
		auto result = Image_view::create(
						  env.device,
						  obj_texture,
						  VK_FORMAT_R8G8B8A8_UNORM,
						  utility_values::default_component_mapping,
						  VK_IMAGE_VIEW_TYPE_2D,
						  {VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1}
					  )
			>> texture_view;
		CHECK_RESULT(result, "Create Image View");
	}

	// Image Sampler
	{
		VkSamplerCreateInfo sampler_info{
			.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter               = VK_FILTER_LINEAR,
			.minFilter               = VK_FILTER_LINEAR,
			.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.mipLodBias              = 0,
			.anisotropyEnable        = env.physical_device.features().samplerAnisotropy,
			.maxAnisotropy           = env.physical_device.properties().limits.maxSamplerAnisotropy,
			.compareEnable           = false,
			.minLod                  = 0,
			.maxLod                  = mip_levels - 1,
			.unnormalizedCoordinates = false
		};
		sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW
			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		auto result = Image_sampler::create(env.device, sampler_info) >> texture_sampler;
		CHECK_RESULT(result, "Create Image Sampler")
	}
}

struct Read_model_index
{
	int vert_idx, normal_idx, uv_idx;

	bool operator==(const Read_model_index& other) const
	{
		return vert_idx == other.vert_idx && normal_idx == other.normal_idx && uv_idx == other.uv_idx;
	}
};

namespace std
{
	template <>
	struct hash<Read_model_index>
	{
		size_t operator()(const Read_model_index& index) const
		{
			size_t seed = 0;
			hash_combine(seed, index.vert_idx);
			hash_combine(seed, index.normal_idx);
			hash_combine(seed, index.uv_idx);
			return seed;
		}

	  private:

		template <typename T>
		static void hash_combine(size_t& seed, const T& value)
		{
			seed ^= hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	};
}

void App_render::read_model(const App_environment& env, const std::string& model_path)
{
	std::vector<Vertex_object> vertices;
	std::vector<uint32_t>      indices;

	// load model
	{
		tinyobj::attrib_t                attrib;
		std::vector<tinyobj::shape_t>    shapes;
		std::vector<tinyobj::material_t> materials;
		std::string                      warning_msg, error_msg;

		auto start_timing = std::chrono::steady_clock::now();

		bool load_success
			= tinyobj::LoadObj(&attrib, &shapes, &materials, &warning_msg, &error_msg, model_path.c_str());
		if (!load_success) throw std::runtime_error(std::format("Load model failed: \n{}: {}", warning_msg, error_msg));

		std::unordered_map<Read_model_index, uint32_t> vertice_set;

		vertice_set.reserve(attrib.vertices.size() / 4);
		vertices.reserve(attrib.vertices.size() / 4);
		indices.reserve(attrib.vertices.size());

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				Read_model_index idx = {index.vertex_index, index.normal_index, index.texcoord_index};

				auto find = vertice_set.find(idx);
				if (find == vertice_set.end())
				{
					Vertex_object v;

					v.pos
						= {attrib.vertices[3 * index.vertex_index + 0],
						   attrib.vertices[3 * index.vertex_index + 1],
						   attrib.vertices[3 * index.vertex_index + 2]};

					v.normal
						= {attrib.normals[3 * index.normal_index + 0],
						   attrib.normals[3 * index.normal_index + 1],
						   attrib.normals[3 * index.normal_index + 2]};

					v.uv
						= {attrib.texcoords[2 * index.texcoord_index + 0],
						   1.0 - attrib.texcoords[2 * index.texcoord_index + 1]};

					vertice_set.emplace(std::make_pair(idx, vertices.size()));
					indices.push_back(vertices.size());
					vertices.push_back(v);
				}
				else
				{
					indices.push_back(find->second);
				}
			}
		}

		auto elapsed_time = std::chrono::duration<float, std::chrono::milliseconds::period>(
								std::chrono::steady_clock::now() - start_timing
		)
								.count();

		tools::print(
			"Loaded model with {} vertices and {} indices in {:2f} milliseconds, compression ratio {:.2f}%",
			vertices.size(),
			indices.size(),
			elapsed_time,
			(float)vertices.size() / indices.size() * 100
		);
	}

	// create buffers
	{
		auto vertex_buffer_result = Buffer::create<Vertex_object>(
										env.allocator,
										vertices.size(),
										VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VMA_MEMORY_USAGE_GPU_ONLY
									)
			>> vertex_buffer;
		CHECK_RESULT(vertex_buffer_result, "Create Vertex Buffer");

		auto index_buffer_result = Buffer::create<uint32_t>(
									   env.allocator,
									   indices.size(),
									   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									   VMA_MEMORY_USAGE_GPU_ONLY
								   )
			>> index_buffer;
		CHECK_RESULT(index_buffer_result, "Create Index Buffer");
	}

	// Transfer data
	{
		Buffer vertex_staging_buffer, index_staging_buffer;

		auto vertex_staging_buffer_result = Buffer::create<Vertex_object>(
												env.allocator,
												vertices.size(),
												VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
												VMA_MEMORY_USAGE_CPU_TO_GPU
											)
			>> vertex_staging_buffer;
		CHECK_RESULT(vertex_staging_buffer_result, "Create Vertex Staging Buffer");

		auto index_staging_buffer_result = Buffer::create<uint32_t>(
											   env.allocator,
											   indices.size(),
											   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
											   VMA_MEMORY_USAGE_CPU_TO_GPU
										   )
			>> index_staging_buffer;
		CHECK_RESULT(index_staging_buffer_result, "Create Index Staging Buffer");

		vertex_staging_buffer->allocation.transfer(vertices);
		index_staging_buffer->allocation.transfer(indices);

		Command_buffer command_buf;

		auto command_buffer_alloc_result
			= Command_buffer::allocate_from(env.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> command_buf;
		CHECK_RESULT(command_buffer_alloc_result, "Create Model Transfer Command Buffer");

		// command buffer: copy buffer
		command_buf.begin_single_submit();
		command_buf.cmd.copy_buffer(vertex_buffer, vertex_staging_buffer);
		command_buf.cmd.copy_buffer(index_buffer, index_staging_buffer);
		auto end_result = command_buf.end();
		if (end_result != VK_SUCCESS)
			throw std::runtime_error(
				std::format("End model transfer command buffer: {}", magic_enum::enum_name(end_result))
			);

		// submit command buffer
		std::array<VkCommandBuffer, 1> cmd_buffers = {*command_buf};

		VkSubmitInfo submit_info{
			.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers    = cmd_buffers.data()
		};
		vkQueueSubmit(env.g_queue, 1, &submit_info, nullptr);
		vkQueueWaitIdle(env.g_queue);
	}
}

void App_render::create_uniform(const App_environment& env, const App_swapchain& swapchain)
{
	for (uint32_t i = 0; i < swapchain.image_count; i++)
	{
		auto result = Buffer::create<Uniform_object>(
						  env.allocator,
						  1,
						  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						  VMA_MEMORY_USAGE_CPU_TO_GPU
					  )
			>> uniform_buffers.emplace_back();
		CHECK_RESULT(result, "Create Uniform Buffer");
	}
}

void App_pipeline::create_descriptors(const App_environment& env, const App_swapchain& swapchain)
{
	// Create Descriptor Pool
	{
		VkDescriptorPoolSize uniform_descriptor_size{
			.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = swapchain.image_count
		};

		VkDescriptorPoolSize sampler_descriptor_size{
			.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = swapchain.image_count
		};

		auto descriptor_sizes = std::to_array({uniform_descriptor_size, sampler_descriptor_size});

		auto result = Descriptor_pool::create(env.device, descriptor_sizes, swapchain.image_count) >> descriptor_pool;

		CHECK_RESULT(result, "Create Descriptor Pool");
	}

	// Creaet Descriptor Sets
	{
		// vector of layouts
		std::vector<VkDescriptorSetLayout> set_layouts(swapchain.image_count, *descriptor_set_layout);

		auto result
			= Descriptor_set::allocate_from(descriptor_pool, swapchain.image_count, set_layouts) >> descriptor_sets;

		CHECK_RESULT(result, "Create Descriptor Sets");
	}
}

void App_synchronization::init(const App_environment& env)
{
	{
		auto result = Semaphore::create(env.device) >> swapchain_ready_semaphore;
		CHECK_RESULT(result, "Create Semaphore (Swapchain Ready)");
	}

	{
		auto result = Semaphore::create(env.device) >> render_complete_semaphore;
		CHECK_RESULT(result, "Create Semaphore (Render Complete)");
	}

	{
		auto result = Fence::create(env.device, true) >> idle_fence;
		CHECK_RESULT(result, "Create Fence (Idle, init:signaled)");
	}
}

void App_imgui_render::destroy()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}