#include "vklib-debug-utils.hpp"
#include "vklib-tools.hpp"

// vklib::Debug_utility
namespace vklib
{

	/* Static Function pointers*/

	PFN_vkCreateDebugUtilsMessengerEXT  Debug_utility::create_debug_utils_pfn  = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT Debug_utility::destroy_debug_utils_pfn = nullptr;

	bool Debug_utility::query_pfn(const Instance& instance)
	{
		if (create_debug_utils_pfn != nullptr && destroy_debug_utils_pfn != nullptr) return true;

		create_debug_utils_pfn
			= (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
		destroy_debug_utils_pfn
			= (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkDestroyDebugUtilsMessengerEXT");

		return create_debug_utils_pfn != nullptr && destroy_debug_utils_pfn != nullptr;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL Debug_utility::api_callback(
		Debug_utility::Msg_severity_bits_t        severity,
		Debug_utility::Msg_type_t                 type,
		const Debug_utility::Msg_callback_data_t* callback_data,
		void*                                     user_data
	)
	{
		auto* callback = (callback_func)user_data;
		callback(severity, type, callback_data);

		return VK_FALSE;
	}

	void Debug_utility::default_callback(
		Msg_severity_bits_t        severity,
		Msg_type_t                 type [[maybe_unused]],
		const Msg_callback_data_t* callback_data
	)
	{
		const char* prefix;

		switch (severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			prefix = "Verbose";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			prefix = "Info";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			prefix = "Warning";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			prefix = "Error";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
		default:
			prefix = "Unknown Error";
			break;
		}

		const char* msg = callback_data->pMessage;

		std::cerr << std::format("{0}: {1}\n", prefix, msg) << std::endl;
	}

	bool Debug_utility::is_supported()
	{
		auto ext_list  = get_supported_extensions();
		bool ext_found = false;
		for (auto [ext_name, _] : ext_list)
			if (strcmp(ext_name, debug_utils_ext_name) == 0)
			{
				ext_found = true;
				break;
			}
		if (!ext_found) return false;

		auto layer_list  = get_supported_layers();
		bool layer_found = false;
		for (auto layer : layer_list)
			if (strcmp(layer.layerName, validation_layer_name) == 0)
			{
				layer_found = true;
				break;
			}
		return layer_found;
	}

	void Debug_utility::clean()
	{
		if (should_delete()) destroy_debug_utils_pfn(*parent, *content, parent.allocator_ptr());
	}

	Result<Debug_utility, VkResult> Debug_utility::create(
		const Instance& instance,
		Msg_severity_t  severity,
		Msg_type_t      msg_type,
		callback_func   callback
	)
	{
		if (!query_pfn(instance))
		{
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		// Create Info
		const VkDebugUtilsMessengerCreateInfoEXT create_info{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			nullptr,
			0,
			severity,
			msg_type,
			api_callback,
			(void*)callback
		};

		VkDebugUtilsMessengerEXT messenger;
		auto result = create_debug_utils_pfn(*instance, &create_info, instance.allocator_ptr(), &messenger);

		if (result != VK_SUCCESS)
			return result;
		else
			return Debug_utility(messenger, instance);
	}

}  // namespace vklib