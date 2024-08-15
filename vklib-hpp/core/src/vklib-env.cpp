#include "vklib/core/env.hpp"

namespace VKLIB_HPP_NAMESPACE
{

#pragma region "Instance"

	Instance::Instance(const vk::InstanceCreateInfo& create_info)
	{
		const auto handle = vk::createInstance(create_info);

		*this = Instance(handle);
	}

	Instance::Instance(
		const vk::ApplicationInfo&      app_info,
		const Array_proxy<const char*>& layers,
		const Array_proxy<const char*>& extensions
	) :
		Instance(
			vk::InstanceCreateInfo().setPEnabledExtensionNames(extensions).setPEnabledLayerNames(layers).setPApplicationInfo(&app_info)
		)
	{
	}

	void Instance::clean()
	{
		if (is_unique()) (*this)->destroy();
	}

	bool Instance::query_instance_extension_support(const char* extension_name)
	{
		const auto supported_list = vk::enumerateInstanceExtensionProperties();

		const auto pred = [extension_name](const vk::ExtensionProperties& properties) -> bool
		{
			return strcmp(extension_name, properties.extensionName) == 0;
		};

		return std::count_if(supported_list.begin(), supported_list.end(), pred) >= 1;
	}

#pragma endregion

#pragma region "Surface"

	Surface::Surface(const Instance& instance, const Window_base& window)
	{
		const auto surface = window.create_surface(instance);
		*this              = Surface(surface, instance);
	}

	void Surface::clean()
	{
		if (is_unique()) parent()->destroySurfaceKHR(*this);
	}

#pragma endregion

#pragma region "Device"

	Device::Device(const Physical_device& physical_device, const vk::DeviceCreateInfo& create_info)
	{
		const auto handle = physical_device.createDevice(create_info);

		*this = Device(handle);
	}

	Device::Device(
		const Physical_device&                 physical_device,
		Array_proxy<vk::DeviceQueueCreateInfo> queue_create_info,
		Array_proxy<const char*>               layers,
		Array_proxy<const char*>               extensions,
		const vk::PhysicalDeviceFeatures&      features,
		const void*                            p_next
	)
	{
		const vk::DeviceCreateInfo create_info({}, queue_create_info, layers, extensions, &features, p_next);
		*this = Device(physical_device, create_info);
	}

	void Device::clean()
	{
		if (is_unique()) (*this)->destroy();
	}

#pragma endregion

#pragma region "Command Pool"

	Command_pool::Command_pool(const Device& device, const vk::CommandPoolCreateInfo& create_info)
	{
		const auto handle = device->createCommandPool(create_info);

		*this = {handle, device};
	}

	Command_pool::Command_pool(const Device& device, uint32_t queue_family_idx, vk::CommandPoolCreateFlags flags)
	{
		const vk::CommandPoolCreateInfo create_info(flags, queue_family_idx);
		*this = {device, create_info};
	}

	void Command_pool::clean()
	{
		if (is_unique()) parent()->destroyCommandPool(*this);
	}

#pragma endregion
}