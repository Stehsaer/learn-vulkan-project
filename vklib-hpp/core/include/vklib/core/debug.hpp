#pragma once

#include <utility>

#include "vklib/core/allocator.hpp"
#include "vklib/core/base.hpp"
#include "vklib/core/cmdbuf.hpp"
#include "vklib/core/env.hpp"
#include "vklib/core/storage.hpp"


namespace VKLIB_HPP_NAMESPACE
{
	// Vulkan Debug Utility
	class Debug_utility : public Child_resource<vk::DebugUtilsMessengerEXT, Instance>
	{
		using Child_resource<vk::DebugUtilsMessengerEXT, Instance>::Child_resource;

		void clean() override;

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

		~Debug_utility() override { clean(); }

		// Callback, generates a Validation_error when called (broken)
		static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(
			VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
			VkDebugUtilsMessageTypeFlagsEXT             message_types,
			const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
			void*                                       user_data
		);
	};

	class Debug_marker
	{
	  private:

		Device device;

		PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTag  = nullptr;
		PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName = nullptr;
		PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBegin      = nullptr;
		PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEnd        = nullptr;
		PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsert     = nullptr;

	  public:

		Debug_marker()  = default;
		~Debug_marker() = default;

		static bool is_supported(const Physical_device& device);
		static bool is_instance_supported();

		// Load function pointers from device
		void load(const Device& device);

		template <typename T>
			requires(vk::isVulkanHandleType<T>::value)
		const Debug_marker& set_object_name(const Mono_resource<T>& object, const std::string& name) const
		{
			using NativeType = T::NativeType;

			if (!vkDebugMarkerSetObjectName) [[likely]]
				return *this;

			VkDebugMarkerObjectNameInfoEXT name_info;
			name_info.object      = reinterpret_cast<uint64_t>(object.template to<NativeType>());
			name_info.objectType  = (VkDebugReportObjectTypeEXT)T::debugReportObjectType;
			name_info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			name_info.pNext       = nullptr;
			name_info.pObjectName = name.c_str();

			vkDebugMarkerSetObjectName(device.to<VkDevice>(), &name_info);

			return *this;
		}

		template <typename T, typename Parent_T>
			requires(vk::isVulkanHandleType<T>::value)
		const Debug_marker& set_object_name(const Child_resource<T, Parent_T>& object, const std::string& name) const
		{
			using NativeType = T::NativeType;

			if (!vkDebugMarkerSetObjectName) [[likely]]
				return *this;

			VkDebugMarkerObjectNameInfoEXT name_info;
			name_info.object      = reinterpret_cast<uint64_t>(object.template to<NativeType>());
			name_info.objectType  = (VkDebugReportObjectTypeEXT)T::debugReportObjectType;
			name_info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			name_info.pObjectName = name.c_str();
			name_info.pNext       = nullptr;

			vkDebugMarkerSetObjectName(device.to<VkDevice>(), &name_info);

			return *this;
		}

		const Debug_marker& set_object_name(const Image& image, const std::string& name) const
		{
			if (!vkDebugMarkerSetObjectName) [[likely]]
				return *this;

			VkDebugMarkerObjectNameInfoEXT name_info;
			name_info.object      = reinterpret_cast<uint64_t>(image.to<VkImage>());
			name_info.objectType  = (VkDebugReportObjectTypeEXT)vk::Image::debugReportObjectType;
			name_info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			name_info.pObjectName = name.c_str();
			name_info.pNext       = nullptr;

			vkDebugMarkerSetObjectName(device.to<VkDevice>(), &name_info);

			return *this;
		}

		const Debug_marker& set_object_name(const Buffer& buffer, const std::string& name) const
		{
			if (!vkDebugMarkerSetObjectName) [[likely]]
				return *this;

			VkDebugMarkerObjectNameInfoEXT name_info;
			name_info.object      = reinterpret_cast<uint64_t>(buffer.to<VkBuffer>());
			name_info.objectType  = (VkDebugReportObjectTypeEXT)vk::Buffer::debugReportObjectType;
			name_info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			name_info.pObjectName = name.c_str();
			name_info.pNext       = nullptr;

			vkDebugMarkerSetObjectName(device.to<VkDevice>(), &name_info);

			return *this;
		}

		void begin_region(const Command_buffer& buffer, const std::string& region_name, glm::vec4 color);
		void insert_marker(const Command_buffer& buffer, const std::string& region_name, glm::vec4 color);
		void end_region(const Command_buffer& buffer);
	};
}