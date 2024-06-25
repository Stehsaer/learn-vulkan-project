#include "environment.hpp"

#define MANUAL_CHOOSE_DEVICE 0
#define FORCE_DEBUG_LAYER 0

Physical_device Environment::helper_select_physical_device(const std::vector<Physical_device>& device_list_input)
{
	static auto condition = [](Physical_device device) -> bool
	{
		const auto  properties = device.getProperties();
		const auto& limits     = properties.limits;

		const bool type_satisfy = properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu
							   || properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;

		return type_satisfy && limits.maxDescriptorSetSamplers >= 3 && limits.maxDescriptorSetInputAttachments >= 5
			&& limits.maxDescriptorSetUniformBuffers >= 3;
	};

	auto       filtered_list = std::views::all(device_list_input) | std::views::filter(condition);
	const auto device_list   = std::vector(filtered_list.begin(), filtered_list.end());

#if !NDEBUG || MANUAL_CHOOSE_DEVICE

	/* DEBUG */

	if (device_list.size() == 0) throw Exception("Can't find suitable physical device");

	if (device_list.size() == 1)
	{
		if (condition(device_list[0])
			&& (device_list[0].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu
				|| device_list[0].getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu))
			return device_list[0];

		throw Exception("Can't find suitable physical device");
	}

	auto type_name = [](vk::PhysicalDeviceType type) -> const char*
	{
		switch (type)
		{
		case vk::PhysicalDeviceType::eDiscreteGpu:
			return "Discrete GPU";
		case vk::PhysicalDeviceType::eIntegratedGpu:
			return "Integrated GPU";
		case vk::PhysicalDeviceType::eCpu:
			return "CPU";
		case vk::PhysicalDeviceType::eVirtualGpu:
			return "Virtual GPU";
		case vk::PhysicalDeviceType::eOther:
			return "Other";
		}
	};

	for (auto i : Range(device_list.size()))
	{
		const auto& device = device_list[i];

		std::cout << std::format(
			"[Device {}]: {} ({})\n",
			i,
			device.getProperties().deviceName.data(),
			type_name(device.getProperties().deviceType)
		);
	}

	std::cout << "Select one GPU: ";

	while (true)
	{
		uint32_t selection;
		std::cin >> selection;

		if (selection < device_list.size())
		{
			if (!condition(device_list[selection]))
			{
				std::cout << "Device doesn't meet minimum requirements! Choose another one.\n";
				continue;
			}

			return device_list[selection];
		}
	}

#else

	/* RELEASE */

	for (const auto& device : device_list)
	{
		if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu && condition(device))
			return device;
	}

	for (const auto& device : device_list)
	{
		if (device.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu && condition(device))
			return device;
	}

	throw Exception("Can't find suitable physical device");

#endif
}

void Environment::create_window()
{
	window = SDL2_window(
		800,
		600,
		"Deferred Rendering",
		(SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI)
	);
}

void Environment::create_instance_debug_utility(bool disable_validation)
{

	std::vector<const char*> instance_extensions = window.get_extension_names();
	std::vector<const char*> layers              = {};

	features.validation_layer_enabled = Debug_utility::is_supported();
	features.debug_marker_enabled     = Debug_marker::is_instance_supported();

#ifdef NDEBUG
	features.validation_layer_enabled = false;
#endif

#if FORCE_DEBUG_LAYER
	features.validation_layer_enabled = true;
#endif

	features.validation_layer_enabled &= !disable_validation;

	// Add Debug Utility layers and extensions
	if (features.validation_layer_enabled)
	{
		instance_extensions.push_back(Debug_utility::debug_utils_ext_name);
		layers.push_back(Debug_utility::validation_layer_name);

		log_msg("Validation Layer ENABLED");
	}

	// Add Debug Marker extension
	if (features.debug_marker_enabled)
	{
		instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		log_msg("Debug Report Extension Supported");
	}

	// Instance
	{
		const vk::ApplicationInfo app_info("Vulkan Application", {}, {}, {}, vk::ApiVersion11);
		instance = Instance(app_info, layers, instance_extensions);
	}

	// Debug Utility
	if (features.validation_layer_enabled)
	{
		debug_utility = Debug_utility(
			instance,
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
				| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
		);
	}
}

void Environment::find_queue_families()
{
	const auto family_properties = physical_device.getQueueFamilyProperties();

	// match graphics queue family
	const auto g_family_match = [=]() -> std::optional<uint32_t>
	{
		for (auto i : Range(family_properties.size()))
		{
			const auto& property = family_properties[i];
			if ((property.queueCount >= 1) && (property.queueFlags & vk::QueueFlagBits::eGraphics)) return i;
		}
		return std::nullopt;
	}();

	const auto c_family_match = [=]() -> std::optional<uint32_t>
	{
		for (auto i : Range(family_properties.size()))
		{
			const auto& property = family_properties[i];
			if ((property.queueCount >= 1) && (property.queueFlags & vk::QueueFlagBits::eCompute)) return i;
		}
		return std::nullopt;
	}();

	// match present queue family
	const auto p_family_match = [=, this]() -> std::optional<uint32_t>
	{
		for (auto i : Range(family_properties.size()))
		{
			auto supported = physical_device.getSurfaceSupportKHR(i, surface);

			if (supported) return i;
		}

		return std::nullopt;
	}();

	if (!g_family_match || !p_family_match || !c_family_match) throw Exception("Can't find suitable family queue");

	g_family_idx = g_family_match.value(), p_family_idx = p_family_match.value(), c_family_idx = c_family_match.value();
	g_family_count = family_properties[g_family_idx].queueCount;

	log_msg("Found Queue Families: G={}/C={}/P={}", g_family_idx, c_family_idx, p_family_idx);
}

void Environment::create_logic_device()
{
	auto queue_priorities = std::to_array({1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});

	std::map<uint32_t, uint32_t> family_idx_map;

	const auto g_queue_offset = family_idx_map[g_family_idx];
	family_idx_map[g_family_idx] += 1;

	const auto t_queue_offset = family_idx_map[g_family_idx];
	family_idx_map[g_family_idx] += 1;

	const auto p_queue_offset = family_idx_map[p_family_idx];
	family_idx_map[p_family_idx]++;

	const auto c_queue_offset = family_idx_map[c_family_idx];
	family_idx_map[c_family_idx]++;

	std::vector<vk::DeviceQueueCreateInfo> queue_create_info;
	queue_create_info.reserve(family_idx_map.size());
	for (const auto& item : family_idx_map) queue_create_info.push_back({{}, item.first, item.second, queue_priorities.data()});

	vk::PhysicalDeviceFeatures requested_features;

	// Request sampler anistropy
	const auto device_features = physical_device.getFeatures();
	const auto device_limits   = physical_device.getProperties().limits;

	if (device_features.samplerAnisotropy) requested_features.samplerAnisotropy = true;
	features.anistropy_enabled = device_features.samplerAnisotropy;
	features.max_anistropy     = device_limits.maxSamplerAnisotropy;

	if (device_features.independentBlend)
		requested_features.independentBlend = true;
	else
		throw Exception("Device Feature Unsupported: Independent Blend");

	std::vector device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	// query debug marker
	if (features.debug_marker_enabled)
	{
		if (!Debug_marker::is_supported(physical_device))
		{
			log_msg("Debug Marker Not Supported");
		}
		else
		{
			device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			log_msg("Debug Marker Device ENABLED");
		}
	}

	device = Device(physical_device, queue_create_info, {}, device_extensions, requested_features, nullptr);

	g_queue = device->getQueue(g_family_idx, g_queue_offset);

	p_queue = device->getQueue(p_family_idx, p_queue_offset);
	t_queue = device->getQueue(g_family_idx, t_queue_offset);
	c_queue = device->getQueue(c_family_idx, c_queue_offset);

	debug_marker.load(device);
}

void Environment::create()
{

	//* Window
	create_window();

	//* Instance & Debug Utility
	create_instance_debug_utility();

	//* Surface
	surface = Surface(instance, window);

	//* Physical Device
	const auto physical_devices = instance->enumeratePhysicalDevices();
	physical_device             = helper_select_physical_device(physical_devices);

	//* Logic Device
	find_queue_families();

	create_logic_device();

	//* Command pool & Command Buffer
	command_pool = Command_pool(device, g_family_idx, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	//* VMA Allocator
	allocator = Vma_allocator(physical_device, device, instance);

	swapchain.create(*this);
}

void Environment::Env_swapchain::create_swapchain(const Environment& env)
{
	auto       available_surface_formats = env.physical_device.getSurfaceFormatsKHR(env.surface);
	const auto capabilities              = env.physical_device.getSurfaceCapabilitiesKHR(env.surface);
	auto       available_present_modes   = env.physical_device.getSurfacePresentModesKHR(env.surface);

	static const std::map<vk::SurfaceFormatKHR, uint32_t> format_priority_list{
		{{vk::Format::eA2R10G10B10UnormPack32, vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear}, 1},
		{{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear}, 1},
		{{vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear},          2},
		{{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear},          2},
	};

	std::sort(
		available_surface_formats.begin(),
		available_surface_formats.end(),
		[=](auto x, auto y)
		{
			const auto find_x = format_priority_list.find(x);
			const auto find_y = format_priority_list.find(y);
			const auto end    = format_priority_list.end();

			if (find_x == end) return false;
			if (find_y == end) return true;

			return find_x->second < find_y->second;
		}
	);

	// select surface format
	surface_format = available_surface_formats[0];
	if (surface_format.format == vk::Format::eA2R10G10B10UnormPack32 || surface_format.format == vk::Format::eA2B10G10R10UnormPack32)
	{
		feature.color_depth_10_enabled = true;
		env.log_msg("10bit Color Depth ENABLED");
	}

	// select image count
	extent.width  = std::clamp(capabilities.currentExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height
		= std::clamp(capabilities.currentExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	// select present mode
	const static std::map<vk::PresentModeKHR, int> present_mode_ranking = {
		{vk::PresentModeKHR::eMailbox,                 0  },
		{vk::PresentModeKHR::eFifoRelaxed,             1  },
		{vk::PresentModeKHR::eFifo,                    2  },
		{vk::PresentModeKHR::eImmediate,               3  },
		{vk::PresentModeKHR::eSharedContinuousRefresh, 100},
		{vk::PresentModeKHR::eSharedDemandRefresh,     100}
	};

	std::ranges::sort(
		available_present_modes,
		[](auto x, auto y)
		{
			return present_mode_ranking.at(x) < present_mode_ranking.at(y);
		}
	);
	auto target_present_mode = available_present_modes[0];

	// select image count
	image_count = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

	env.log_msg("Swapchain Size: [{}, {}], {} image{}", extent.width, extent.height, image_count, image_count > 1 ? "s" : "");

	auto queue_families    = std::to_array({env.g_family_idx, env.p_family_idx});
	auto queue_family_span = std::span(queue_families.begin(), env.g_family_idx == env.p_family_idx ? 1 : 2);

	swapchain = Swapchain(
		env.device,
		env.surface,
		surface_format,
		target_present_mode,
		extent,
		image_count,
		queue_family_span,
		capabilities.currentTransform,
		swapchain
	);
}

void Environment::Env_swapchain::create_images(const Environment& env)
{
	// get images
	image_handles = swapchain.get_images();

	// create views
	image_views.clear();
	for (auto i : Range(image_count))
		image_views.emplace_back(
			env.device,
			image_handles[i],
			surface_format.format,
			vk::ImageViewType::e2D,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
}

void Environment::Env_swapchain::create(Environment& env)
{
	create_swapchain(env);
	create_images(env);

	env.command_buffer = Command_buffer::allocate_multiple_from(env.command_pool, image_count);
}
