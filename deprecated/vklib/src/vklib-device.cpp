#include "vklib-device.hpp"

// vklib::Physical_device
namespace vklib
{
	Physical_device::Physical_device(VkPhysicalDevice device) :
		Base_copyable<VkPhysicalDevice>(device),
		device_properties(new Physical_device_properties())
	{
		acquire_physical_device_info();
	}

	void Physical_device::acquire_physical_device_info()
	{
		auto* device = *content;

		vkGetPhysicalDeviceProperties(device, &device_properties->properties);
		vkGetPhysicalDeviceFeatures(device, &device_properties->features);

		uint32_t extension_count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
		device_properties->extension_properties.resize(extension_count);
		vkEnumerateDeviceExtensionProperties(
			device,
			nullptr,
			&extension_count,
			device_properties->extension_properties.data()
		);
	}

	std::vector<Physical_device> Physical_device::enumerate_from(const Instance& instance)
	{
		uint32_t physical_device_count;
		vkEnumeratePhysicalDevices(*instance, &physical_device_count, nullptr);

		std::vector<VkPhysicalDevice> device_list(physical_device_count);
		vkEnumeratePhysicalDevices(*instance, &physical_device_count, device_list.data());

		std::vector<Physical_device> physical_device_list;
		physical_device_list.reserve(physical_device_count);

		for (const auto& physical_device : device_list)
		{
			physical_device_list.push_back(Physical_device(physical_device));
		}

		return physical_device_list;
	}

	std::vector<VkQueueFamilyProperties> Physical_device::get_queue_family_properties() const
	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(*content, &count, nullptr);

		std::vector<VkQueueFamilyProperties> list(count);
		vkGetPhysicalDeviceQueueFamilyProperties(*content, &count, list.data());

		return list;
	}

	VkFormatProperties Physical_device::get_format_properties(VkFormat format) const
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(*content, format, &properties);
		return properties;
	}

	std::optional<uint32_t> Physical_device::match_queue_family(
		const std::function<bool(VkQueueFamilyProperties)>& condition
	) const
	{
		const auto list = get_queue_family_properties();

		for (uint32_t i = 0; i < list.size(); i++)
		{
			if (condition(list[i])) return i;
		}

		return std::nullopt;
	}

	std::optional<uint32_t> Physical_device::match_present_queue_family(const Surface& surface) const
	{
		const auto list = get_queue_family_properties();

		for (size_t i = 0; i < list.size(); i++)
		{
			VkBool32 supported;
			vkGetPhysicalDeviceSurfaceSupportKHR(*content, i, *surface, &supported);

			if (supported) return i;
		}

		return std::nullopt;
	}

	bool Physical_device::check_extensions_support(std::span<const char*> extension_list) const
	{
		auto extension_supported = [this](const char* val) -> bool
		{
			for (const auto& supported_ext : device_properties->extension_properties)
			{
				if (strcmp(supported_ext.extensionName, val) == 0) return true;
			}
			return false;
		};

		auto filtered = extension_list | std::views::filter(extension_supported);

		return (size_t)std::ranges::distance(filtered) == extension_list.size();
	}

	Swapchain_detail Physical_device::get_swapchain_detail(const Surface& surface) const
	{
		Swapchain_detail detail;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*content, *surface, &detail.capabilities);

		uint32_t format_count, present_mode_count;

		vkGetPhysicalDeviceSurfaceFormatsKHR(*content, *surface, &format_count, nullptr);
		vkGetPhysicalDeviceSurfacePresentModesKHR(*content, *surface, &present_mode_count, nullptr);

		detail.formats.resize(format_count);
		detail.present_modes.resize(present_mode_count);

		if (format_count > 0)
			vkGetPhysicalDeviceSurfaceFormatsKHR(*content, *surface, &format_count, detail.formats.data());
		if (present_mode_count > 0)
			vkGetPhysicalDeviceSurfacePresentModesKHR(
				*content,
				*surface,
				&present_mode_count,
				detail.present_modes.data()
			);

		return detail;
	}

	VkPhysicalDeviceMemoryProperties Physical_device::get_memory_properties() const
	{
		VkPhysicalDeviceMemoryProperties properties;
		vkGetPhysicalDeviceMemoryProperties(*content, &properties);
		return properties;
	}
}

// vklib::Device
namespace vklib
{
	Result<Device, VkResult> Device::create(
		const Instance&                          instance,
		const Physical_device&                   physical_device,
		std::span<const VkDeviceQueueCreateInfo> queue_create_info,
		std::span<const char*>                   layers,
		std::span<const char*>                   extensions,
		const VkPhysicalDeviceFeatures&          features
	)
	{
		VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0};

		create_info.pQueueCreateInfos    = queue_create_info.data();
		create_info.queueCreateInfoCount = queue_create_info.size();

		create_info.enabledExtensionCount   = extensions.size();
		create_info.ppEnabledExtensionNames = extensions.data();

		create_info.enabledLayerCount   = layers.size();
		create_info.ppEnabledLayerNames = layers.data();

		create_info.pEnabledFeatures = &features;

		VkDevice device;
		auto     result = vkCreateDevice(*physical_device, &create_info, instance.allocator_ptr(), &device);

		if (result != VK_SUCCESS)
			return result;
		else
			return Device(device, instance);  //.internal_set_base_physical_device(physical_device);
	}

	void Device::clean()
	{
		if (should_delete()) vkDestroyDevice(*content, parent.allocator_ptr());
	}

	VkQueue Device::get_queue_handle(uint32_t family_idx, uint32_t queue_idx) const
	{
		VkQueue q;
		vkGetDeviceQueue(*content, family_idx, queue_idx, &q);
		return q;
	}

	void Device::wait_idle() const
	{
		vkDeviceWaitIdle(*content);
	}

	// Device&& Device::internal_set_base_physical_device(const Physical_device& val) &&
	// {
	// 	this->base_physical_device = val;
	// 	return std::forward<Device&&>(*this);
	// }
}
