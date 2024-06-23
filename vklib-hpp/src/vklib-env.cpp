#include "vklib-env.hpp"

namespace VKLIB_HPP_NAMESPACE
{

#pragma region "Instance"

	Instance::Instance(
		const vk::ApplicationInfo&     app_info,
		Const_array_proxy<const char*> layers,
		Const_array_proxy<const char*> extensions
	)
	{
		vk::InstanceCreateInfo create_info;
		create_info.setPEnabledExtensionNames(extensions);
		create_info.setPEnabledLayerNames(layers);
		create_info.setPApplicationInfo(&app_info);

		auto handle = vk::createInstance(create_info);
		*this       = Instance(handle);
	}

	void Instance::clean()
	{
		if (is_unique()) (*this)->destroy();
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

	Device::Device(
		const Physical_device&                       physical_device,
		Const_array_proxy<vk::DeviceQueueCreateInfo> queue_create_info,
		Const_array_proxy<const char*>               layers,
		Const_array_proxy<const char*>               extensions,
		const vk::PhysicalDeviceFeatures&            features,
		const void*                                  p_next
	)
	{
		const vk::DeviceCreateInfo create_info({}, queue_create_info, layers, extensions, &features, p_next);

		auto handle = physical_device.createDevice(create_info);
		*this       = Device(handle);
	}

	void Device::clean()
	{
		if (is_unique()) (*this)->destroy();
	}

#pragma endregion

#pragma region "Command Pool"

	Command_pool::Command_pool(const Device& device, uint32_t queue_family_idx, vk::CommandPoolCreateFlags flags)
	{
		const vk::CommandPoolCreateInfo create_info(flags, queue_family_idx);

		auto handle = device->createCommandPool(create_info);
		*this       = Command_pool(handle, device);
	}

	void Command_pool::clean()
	{
		if (is_unique()) parent()->destroyCommandPool(*this);
	}

#pragma endregion
}