#pragma once

#include "vklib-base.hpp"
#include "vklib-common.h"
#include "vklib-instance.hpp"
#include "vklib-surface.hpp"

namespace vklib
{
	struct Swapchain_detail
	{
		VkSurfaceCapabilitiesKHR        capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR>   present_modes;
	};

	class Physical_device : public Base_copyable<VkPhysicalDevice>
	{
	  public:

		virtual ~Physical_device() = default;

		static std::vector<Physical_device> enumerate_from(const Instance& instance);

		const VkPhysicalDeviceFeatures&           features() const { return device_properties->features; }
		const VkPhysicalDeviceProperties&         properties() const { return device_properties->properties; }
		const std::vector<VkExtensionProperties>& extension_properties() const
		{
			return device_properties->extension_properties;
		}

		VkFormatProperties get_format_properties(VkFormat format) const;

		std::vector<VkQueueFamilyProperties> get_queue_family_properties() const;

		// Utility: Find the matching queue family in list
		std::optional<uint32_t> match_queue_family(const std::function<bool(VkQueueFamilyProperties)>& condition) const;

		// Utility: Find the present queue family index
		std::optional<uint32_t> match_present_queue_family(const Surface& surface) const;

		// Utility: Get Swapchain capability detail
		Swapchain_detail get_swapchain_detail(const Surface& surface) const;

		// Utility: Check if all extensions in the given list are supported
		bool check_extensions_support(std::span<const char*> extension_list) const;

		VkPhysicalDeviceMemoryProperties get_memory_properties() const;

	  private:

		struct Physical_device_properties
		{
			VkPhysicalDeviceProperties         properties;
			VkPhysicalDeviceFeatures           features;
			std::vector<VkExtensionProperties> extension_properties;
		};

		std::shared_ptr<Physical_device_properties> device_properties;

		Physical_device(VkPhysicalDevice device);

		void acquire_physical_device_info();

		using Base_copyable<VkPhysicalDevice>::Base_copyable;

		void clean() override {}
	};

	class Device : public Base_copyable_parented<VkDevice, Instance>
	{
	  public:

		using Base_copyable_parented<VkDevice, Instance>::Base_copyable_parented;

		// Create device
		static Result<Device, VkResult> create(
			const Instance&                          instance,
			const Physical_device&                   physical_device,
			std::span<const VkDeviceQueueCreateInfo> queue_create_info,
			std::span<const char*>                   layers,
			std::span<const char*>                   extensions,
			const VkPhysicalDeviceFeatures&          features
		);

		VkQueue get_queue_handle(uint32_t family_idx, uint32_t queue_idx) const;
		void    wait_idle() const;

		const VkAllocationCallbacks* allocator_ptr() const { return parent.allocator_ptr(); }
		// const Physical_device&		 get_device() const { return base_physical_device; }

		void clean() override;
		~Device() override { clean(); }
	};
}