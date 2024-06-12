#pragma once

#include "vklib-common.h"
#include "vklib-instance.hpp"
#include "vklib-tools.hpp"

namespace vklib
{
	class Debug_utility : public Base_copyable_parented<VkDebugUtilsMessengerEXT, Instance>
	{
	  public:

		/* Type Definitions */

		using Msg_severity_t      = VkDebugUtilsMessageSeverityFlagsEXT;
		using Msg_severity_bits_t = VkDebugUtilsMessageSeverityFlagBitsEXT;
		using Msg_type_t          = VkDebugUtilsMessageTypeFlagsEXT;
		using Msg_callback_data_t = VkDebugUtilsMessengerCallbackDataEXT;
		using callback_func
			= void (*)(Msg_severity_bits_t severity, Msg_type_t type, const Msg_callback_data_t* callback_data);

		/* Constant strings */

		static constexpr const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* debug_utils_ext_name  = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

		// Check if debug utility is supported
		[[nodiscard]] static bool is_supported();

		// Default callback, useful if you dont want to write your own one
		static void default_callback(
			Msg_severity_bits_t        severity,
			Msg_type_t                 type,
			const Msg_callback_data_t* callback_data
		);

		// Create a Debug Utility instance
		[[nodiscard]] static Result<Debug_utility, VkResult> create(
			const Instance& instance,
			Msg_severity_t  severity,
			Msg_type_t      msg_type,
			callback_func   callback
		);

		using Base_copyable_parented<VkDebugUtilsMessengerEXT, Instance>::Base_copyable_parented;
		using Base_copyable_parented<VkDebugUtilsMessengerEXT, Instance>::operator=;

		void clean() override;

		~Debug_utility() override { clean(); }

	  private:

		static VKAPI_ATTR VkBool32 VKAPI_CALL api_callback(
			Msg_severity_bits_t        severity,
			Msg_type_t                 type,
			const Msg_callback_data_t* callback_data,
			void*                      user_data
		);
		static bool query_pfn(const Instance& instance);

		static PFN_vkCreateDebugUtilsMessengerEXT  create_debug_utils_pfn;
		static PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_pfn;
	};
}  // namespace vklib