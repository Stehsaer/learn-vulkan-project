#include "app-basic.hpp"

#define MANUAL_CHOOSE_DEVICE 0
#define FORCE_DEBUG_LAYER 0

Physical_device App_environment::helper_select_physical_device(const std::vector<Physical_device>& device_list_input)
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

	if (device_list.size() == 0) throw General_exception("Can't find suitable physical device");

	if (device_list.size() == 1)
	{
		if (condition(device_list[0])
			&& (device_list[0].getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu
				|| device_list[0].getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu))
			return device_list[0];

		throw General_exception("Can't find suitable physical device");
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

	throw General_exception("Can't find suitable physical device");

#endif
}

void App_environment::create_window()
{
	window = SDL2_window(
		800,
		600,
		"Deferred Rendering",
		(SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI)
	);
}

void App_environment::create_instance_debug_utility(bool disable_validation)
{

	std::vector<const char*> instance_extensions = window.get_extension_names();
	std::vector<const char*> layers              = {};

	bool enable_validation_layer = Debug_utility::is_supported();

#ifdef NDEBUG
	enable_validation_layer = false;
#endif

#if FORCE_DEBUG_LAYER
	enable_validation_layer = true;
#endif

	enable_validation_layer &= !disable_validation;

	// Add Debug Utility layers and extensions
	if (enable_validation_layer)
	{
		instance_extensions.push_back(Debug_utility::debug_utils_ext_name);
		layers.push_back(Debug_utility::validation_layer_name);
	}

	// Instance
	{
		const vk::ApplicationInfo app_info("Vulkan Application", {}, {}, {}, vk::ApiVersion11);
		instance = Instance(app_info, layers, instance_extensions);
	}

	// Debug Utility
	if (enable_validation_layer)
	{
		debug_utility = Debug_utility(
			instance,
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
				| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
		);
	}
}

void App_environment::find_queue_families()
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

	if (!g_family_match || !p_family_match || !c_family_match)
		throw General_exception("Can't find suitable family queue");

	g_family_idx = g_family_match.value(), p_family_idx = p_family_match.value(), c_family_idx = c_family_match.value();
	g_family_count = family_properties[g_family_idx].queueCount;
}

void App_environment::create_logic_device()
{

	auto queue_priorities = std::to_array({1.0f, 1.0f, 1.0f});

	const vk::DeviceQueueCreateInfo g_queue_create_info(
		{},
		g_family_idx,
		std::min(g_family_idx == p_family_idx ? 3u : 2u, g_family_count),
		queue_priorities.data()
	);

	const vk::DeviceQueueCreateInfo p_queue_create_info({}, p_family_idx, 1, queue_priorities.data());

	const vk::DeviceQueueCreateInfo c_queue_create_info({}, c_family_idx, 1, queue_priorities.data());

	auto queue_create_info = std::to_array({g_queue_create_info, p_queue_create_info, c_queue_create_info});

	// Request sampler anistropy
	vk::PhysicalDeviceFeatures requested_features;

	auto device_features = physical_device.getFeatures();
	if (device_features.samplerAnisotropy) requested_features.samplerAnisotropy = true;

	if (device_features.independentBlend)
		requested_features.independentBlend = true;
	else
		throw General_exception("Device Feature Unsupported: Independent Blend");

	const auto device_extensions = std::to_array({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

	device = Device(
		physical_device,
		std::span(queue_create_info.data(), 1 + (g_family_idx != p_family_idx)),
		{},
		device_extensions,
		requested_features,
		nullptr
	);

	auto p_queue_idx = [this]() -> int
	{
		if (g_family_idx != p_family_idx) return 0;  // separate queue
		if (g_family_count == 1) return 0;           // only one queue
		return 1;                                    // more than 1 queue
	}();

	auto t_queue_idx = [this]() -> int
	{
		// 1 queue
		if (g_family_count == 1) return 0;

		// 2 queues
		if (g_family_count == 2)
		{
			if (g_family_idx == p_family_idx) return 0;  // one for G/T, one for P
			return 1;                                    // one for G, one for T
		}

		// more than 2 queues
		if (g_family_idx == p_family_idx) return 2;
		return 1;
	}();

	g_queue = device->getQueue(g_family_idx, 0);
	p_queue = device->getQueue(p_family_idx, p_queue_idx);
	t_queue = device->getQueue(g_family_idx, t_queue_idx);
	c_queue = device->getQueue(c_family_idx, 0);
}

void App_environment::create()
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
}
