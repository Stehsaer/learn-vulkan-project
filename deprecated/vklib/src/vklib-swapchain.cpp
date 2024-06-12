#include "vklib-swapchain.hpp"

namespace vklib
{
	Result<Swapchain, VkResult> Swapchain::create(
		const Device&                 device,
		const Surface&                surface,
		const VkSurfaceFormatKHR&     surface_format,
		const VkPresentModeKHR&       present_mode,
		VkExtent2D                    extent,
		uint32_t                      image_count,
		std::span<uint32_t>           queue_families,
		VkSurfaceTransformFlagBitsKHR transform,
		VkSwapchainKHR                old_handle,
		bool                          clip,
		VkImageUsageFlags             image_usage,
		VkCompositeAlphaFlagBitsKHR   composite_alpha
	)
	{
		// Create info
		VkSwapchainCreateInfoKHR create_info{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

		create_info.surface          = *surface;
		create_info.imageFormat      = surface_format.format;
		create_info.imageColorSpace  = surface_format.colorSpace;
		create_info.imageExtent      = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage       = image_usage;
		create_info.imageSharingMode
			= queue_families.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
		create_info.minImageCount         = image_count;
		create_info.presentMode           = present_mode;
		create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
		create_info.pQueueFamilyIndices   = queue_families.data();
		create_info.preTransform          = transform;
		create_info.compositeAlpha        = composite_alpha;
		create_info.oldSwapchain          = old_handle;
		create_info.clipped               = clip;

		VkSwapchainKHR swapchain;
		auto           result = vkCreateSwapchainKHR(*device, &create_info, device.allocator_ptr(), &swapchain);

		if (result != VK_SUCCESS)
			return result;
		else
			return Swapchain(swapchain, device);
	}

	void Swapchain::clean()
	{
		if (should_delete()) vkDestroySwapchainKHR(*parent, *content, parent.allocator_ptr());
	}

	std::vector<Image> Swapchain::get_images() const
	{
		uint32_t             count;
		std::vector<VkImage> c_images;

		vkGetSwapchainImagesKHR(*parent, *content, &count, nullptr);
		c_images.resize(count);
		vkGetSwapchainImagesKHR(*parent, *content, &count, c_images.data());

		std::vector<Image> images;
		images.reserve(count);
		for (size_t i = 0; i < count; i++) images.push_back(Image::raw_image(c_images[i]));

		return images;
	}

	Result<uint32_t, VkResult> Swapchain::acquire_next_image(
		const std::optional<Semaphore>& signal_semaphore,
		const std::optional<Fence>&     signal_fence,
		uint64_t                        timeout
	)
	{
		uint32_t index;
		auto     result = vkAcquireNextImageKHR(
            *parent,
            *content,
            timeout,
            signal_semaphore ? *signal_semaphore.value() : VK_NULL_HANDLE,
            signal_fence ? *signal_fence.value() : VK_NULL_HANDLE,
            &index
        );

		if (result != VK_SUCCESS)
			return result;
		else
			return index;
	}
}