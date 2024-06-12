#include "application.hpp"

void App_environment::create()
{
	std::vector<const char*> instance_extensions = glfw_extension_names();
	std::vector<const char*> layers              = {};

	// Create window
	{
		const auto* video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

		auto result
			= Window::create(video_mode->width / 2, video_mode->height / 2, "Deferred Shading", nullptr, true, true)
			>> window;
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
			[](auto property) -> bool
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

		t_queue_available = physical_device.get_queue_family_properties()[g_queue_family].queueCount > 1;

		// queue create infos
		auto queue_priority = std::to_array({1.0f, 1.0f});

		VkDeviceQueueCreateInfo g_queue_create_info{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = g_queue_family,
			.queueCount       = t_queue_available ? 2u : 1u,
			.pQueuePriorities = queue_priority.data()
		};

		VkDeviceQueueCreateInfo p_queue_create_info{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = p_queue_family,
			.queueCount       = 1,
			.pQueuePriorities = queue_priority.data()
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

		// Acquire queue handles
		g_queue = device.get_queue_handle(g_queue_family, 0);
		p_queue = device.get_queue_handle(p_queue_family, 0);
		t_queue = t_queue_available ? device.get_queue_handle(g_queue_family, 1) : g_queue;
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
