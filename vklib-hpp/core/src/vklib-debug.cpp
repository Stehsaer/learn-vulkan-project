#include "vklib/core/debug.hpp"

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

VKAPI_ATTR void VKAPI_CALL
vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks const* pAllocator)
{
	return pfn_vk_destroy_debug_utils_messenger_ext(instance, messenger, pAllocator);
}

static bool load_debug_messenger_pfn(const vklib::Instance& instance)
{
	if (pfn_vk_create_debug_utils_messenger_ext != nullptr && pfn_vk_destroy_debug_utils_messenger_ext != nullptr) return true;

	pfn_vk_create_debug_utils_messenger_ext
		= reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(instance->getProcAddr("vkCreateDebugUtilsMessengerEXT"));

	pfn_vk_destroy_debug_utils_messenger_ext
		= reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(instance->getProcAddr("vkDestroyDebugUtilsMessengerEXT"));

	if (pfn_vk_create_debug_utils_messenger_ext == nullptr) return false;
	if (pfn_vk_destroy_debug_utils_messenger_ext == nullptr) return false;

	return true;
}

namespace VKLIB_HPP_NAMESPACE
{
#pragma region "Debug Utility"

	Debug_utility::Debug_utility(const Instance& instance, Message_severity_t severity, Message_type_t msg_type, callback_t* callback)
	{
		vk::DebugUtilsMessengerCreateInfoEXT create_info;
		create_info.setMessageSeverity(severity);
		create_info.setMessageType(msg_type);
		create_info.setPfnUserCallback(callback);

		if (!load_debug_messenger_pfn(instance)) throw Exception("Debug messenger functions not found");

		auto handle = instance->createDebugUtilsMessengerEXT(create_info);
		*this       = Debug_utility(handle, instance);
	}

	void Debug_utility::clean()
	{
		if (is_unique() && load_debug_messenger_pfn(this->parent()))
		{
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
		const auto ext_list = vk::enumerateInstanceExtensionProperties();

		bool ext_found = false;
		for (auto [ext_name, _] : ext_list)
			if (strcmp(ext_name.data(), debug_utils_ext_name) == 0)
			{
				ext_found = true;
				break;
			}
		if (!ext_found) return false;

		const auto layer_list = vk::enumerateInstanceLayerProperties();

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

#pragma region "Debug Marker"

	bool Debug_marker::is_supported(const Physical_device& device)
	{
		const auto device_extensions = device.enumerateDeviceExtensionProperties();

		return std::ranges::any_of(
			device_extensions,
			[](const auto& property)
			{
				return strcmp(property.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0;
			}
		);
	}

	bool Debug_marker::is_instance_supported()
	{
		const auto ext_list = vk::enumerateInstanceExtensionProperties();

		for (auto [ext_name, _] : ext_list)
			if (strcmp(ext_name.data(), VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
			{
				return true;
			}
		return false;
	}

	void Debug_marker::load(const Device& device)
	{
		vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)device->getProcAddr("vkDebugMarkerSetObjectTagEXT");
		vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)device->getProcAddr("vkDebugMarkerSetObjectNameEXT");
		vkCmdDebugMarkerBegin      = (PFN_vkCmdDebugMarkerBeginEXT)device->getProcAddr("vkCmdDebugMarkerBeginEXT");
		vkCmdDebugMarkerEnd        = (PFN_vkCmdDebugMarkerEndEXT)device->getProcAddr("vkCmdDebugMarkerEndEXT");
		vkCmdDebugMarkerInsert     = (PFN_vkCmdDebugMarkerInsertEXT)device->getProcAddr("vkCmdDebugMarkerInsertEXT");

		this->device = device;
	}

	void Debug_marker::begin_region(const Command_buffer& buffer, const std::string& region_name, glm::vec4 color)
	{
		if (!vkCmdDebugMarkerBegin) [[likely]]
			return;

		VkDebugMarkerMarkerInfoEXT marker_info = {};

		marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(marker_info.color, &color, sizeof(float) * 4);
		marker_info.pMarkerName = region_name.c_str();

		vkCmdDebugMarkerBegin(buffer.raw(), &marker_info);
	}

	void Debug_marker::insert_marker(const Command_buffer& buffer, const std::string& region_name, glm::vec4 color)
	{
		if (!vkCmdDebugMarkerInsert) [[likely]]
			return;

		VkDebugMarkerMarkerInfoEXT marker_info = {};

		marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(marker_info.color, &color, sizeof(float) * 4);
		marker_info.pMarkerName = region_name.c_str();

		vkCmdDebugMarkerInsert(buffer.raw(), &marker_info);
	}

	void Debug_marker::end_region(const Command_buffer& buffer)
	{
		if (!vkCmdDebugMarkerEnd) [[likely]]
			return;

		vkCmdDebugMarkerEnd(buffer.raw());
	}

#pragma endregion
}