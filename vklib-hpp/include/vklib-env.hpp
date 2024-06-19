#pragma once

//* vklib-env.hpp
//* Contains:
//	- Window
//	- Instance
//	- Debug Utility
//	- Surface
//	- Physical Device
//	- Device
//	- Queue
//	- Command Pool

#include <utility>

#include "vklib-base.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	/* ====== Environment Helpers ====== */

	// Vulkan Instance
	class Instance : public Mono_resource<vk::Instance>
	{
		using Mono_resource<vk::Instance>::Mono_resource;

		std::optional<vk::AllocationCallbacks> allocation_callback;

	  public:

		Instance(
			const vk::ApplicationInfo&     app_info,
			Const_array_proxy<const char*> layers,
			Const_array_proxy<const char*> extensions
		);

		void clean() override;
		~Instance() override { clean(); }
	};

	struct Window_exception : public Exception
	{
		using Exception::Exception;
	};

	// Base class for all window implementations
	class Window_base
	{
	  public:

		virtual vk::SurfaceKHR           create_surface(const Instance& instance) const = 0;
		virtual std::tuple<int, int>     get_framebuffer_size() const                   = 0;
		virtual std::vector<const char*> get_extension_names() const                    = 0;
	};

	// Vulkan Debug Utility
	class Debug_utility : public Child_resource<vk::DebugUtilsMessengerEXT, Instance>
	{
		using Child_resource<vk::DebugUtilsMessengerEXT, Instance>::Child_resource;

	  public:

		/* ===== Type Definitions =====  */

		using Message_severity_t   = vk::DebugUtilsMessageSeverityFlagsEXT;
		using Message_severity_bit = vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using Message_type_t       = vk::DebugUtilsMessageTypeFlagsEXT;
		using Callback_data_t      = vk::DebugUtilsMessengerCallbackDataEXT;

		using callback_t = VkBool32(
			VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
			VkDebugUtilsMessageTypeFlagsEXT             message_types,
			const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
			void*                                       user_data
		);

		/* ===== Static Constants ===== */

		// Validation Layer Name, add to array of layers when creating Instance
		static constexpr const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";

		// Debug Utility Extension Name, add to array of extensions when creating Instance
		static constexpr const char* debug_utils_ext_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

		// Query if extension is supported
		static bool is_supported();

		/* ===== Constructor & Destructor ===== */

		Debug_utility(
			const Instance&    instance,
			Message_severity_t severity,
			Message_type_t     msg_type,
			callback_t*        callback = validation_callback
		);

		void clean() override;
		~Debug_utility() override { clean(); }

		// Callback, generates a Validation_error when called (broken)
		static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(
			VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
			VkDebugUtilsMessageTypeFlagsEXT             message_types,
			const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
			void*                                       user_data
		);
	};

	// Window Surface
	class Surface : public Child_resource<vk::SurfaceKHR, Instance>
	{
		using Child_resource<vk::SurfaceKHR, Instance>::Child_resource;

	  public:

		Surface(const Instance& instance, const Window_base& window);

		void clean() override;
		~Surface() override { clean(); }
	};

	// Physical Device, no RAII required
	using Physical_device = vk::PhysicalDevice;

	class Device : public Mono_resource<vk::Device>
	{
		using Mono_resource<vk::Device>::Mono_resource;

	  public:

		Device(
			const Physical_device&                       physical_device,
			Const_array_proxy<vk::DeviceQueueCreateInfo> queue_create_info,
			Const_array_proxy<const char*>               layers,
			Const_array_proxy<const char*>               extensions,
			const vk::PhysicalDeviceFeatures&            features,
			const void*                                  p_next = nullptr
		);

		void clean() override;
		~Device() override { clean(); }

		/* Functions */
		void wait_idle() const { data->waitIdle(); }
	};

	// Vulkan Queue, no RAII required
	using Queue = vk::Queue;

	// Command Pool
	class Command_pool : public Child_resource<vk::CommandPool, Device>
	{
		using Child_resource<vk::CommandPool, Device>::Child_resource;

	  public:

		Command_pool(const Device& device, uint32_t queue_family_idx, vk::CommandPoolCreateFlags flags = {});

		void clean() override;
		~Command_pool() override { clean(); }
	};

}