#include "vklib-env.hpp"

static PFN_vkCreateDebugUtilsMessengerEXT  pfn_vk_create_debug_utils_messenger_ext  = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT pfn_vk_destroy_debug_utils_messenger_ext = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
	VkInstance                                instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks*              pAllocator,
	VkDebugUtilsMessengerEXT*                 pMessenger
)
{
	return pfn_vk_create_debug_utils_messenger_ext(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
	VkInstance                   instance,
	VkDebugUtilsMessengerEXT     messenger,
	VkAllocationCallbacks const* pAllocator
)
{
	return pfn_vk_destroy_debug_utils_messenger_ext(instance, messenger, pAllocator);
}

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

#pragma region "Debug Utility"

	Debug_utility::Debug_utility(
		const Instance&    instance,
		Message_severity_t severity,
		Message_type_t     msg_type,
		callback_t*        callback
	)
	{
		vk::DebugUtilsMessengerCreateInfoEXT create_info;
		create_info.setMessageSeverity(severity);
		create_info.setMessageType(msg_type);
		create_info.setPfnUserCallback(callback);

		pfn_vk_create_debug_utils_messenger_ext = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			instance->getProcAddr("vkCreateDebugUtilsMessengerEXT")
		);

		if (pfn_vk_create_debug_utils_messenger_ext == nullptr) throw Exception("Function: vkCreateDebugUtilsMessengerEXT not found ");

		auto handle = instance->createDebugUtilsMessengerEXT(create_info);
		*this       = Debug_utility(handle, instance);
	}

	void Debug_utility::clean()
	{
		if (is_unique())
		{
			pfn_vk_destroy_debug_utils_messenger_ext = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				parent()->getProcAddr("vkDestroyDebugUtilsMessengerEXT")
			);

			if (pfn_vk_destroy_debug_utils_messenger_ext == nullptr) return;

			parent()->destroyDebugUtilsMessengerEXT(*this);
		}
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL Debug_utility::validation_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity [[maybe_unused]],
		VkDebugUtilsMessageTypeFlagsEXT             message_types [[maybe_unused]],
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data [[maybe_unused]],
		void*                                       user_data [[maybe_unused]]
	)
	{
		std::cerr << callback_data->pMessage << std::endl;
		return true;
	}

	bool Debug_utility::is_supported()
	{
		auto ext_list = vk::enumerateInstanceExtensionProperties();

		bool ext_found = false;
		for (auto [ext_name, _] : ext_list)
			if (strcmp(ext_name.data(), debug_utils_ext_name) == 0)
			{
				ext_found = true;
				break;
			}
		if (!ext_found) return false;

		auto layer_list = vk::enumerateInstanceLayerProperties();

		bool layer_found = false;
		for (auto layer : layer_list)
			if (strcmp(layer.layerName.data(), validation_layer_name) == 0)
			{
				layer_found = true;
				break;
			}
		return layer_found;
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