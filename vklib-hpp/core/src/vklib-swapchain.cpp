#include "vklib-swapchain.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	Swapchain::Swapchain(
		const Device&                   device,
		const Surface&                  surface,
		const vk::SurfaceFormatKHR&     surface_format,
		const vk::PresentModeKHR&       present_mode,
		vk::Extent2D                    extent,
		uint32_t                        image_count,
		std::span<const uint32_t>       queue_families,
		vk::SurfaceTransformFlagBitsKHR transform,
		vk::SwapchainKHR                old_handle,
		bool                            clip,
		vk::ImageUsageFlags             image_usage,
		vk::CompositeAlphaFlagBitsKHR   composite_alpha
	)
	{
		vk::SwapchainCreateInfoKHR create_info(
			{},
			surface,
			image_count,
			surface_format.format,
			surface_format.colorSpace,
			extent,
			1,
			image_usage,
			queue_families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive
		);

		create_info.setPresentMode(present_mode)
			.setPreTransform(transform)
			.setOldSwapchain(old_handle)
			.setClipped(clip)
			.setCompositeAlpha(composite_alpha)
			.setQueueFamilyIndices(queue_families);

		auto handle = device->createSwapchainKHR(create_info);
		*this       = Swapchain(handle, device);
	}

	void Swapchain::clean()
	{
		if (is_unique()) parent()->destroySwapchainKHR(*this);
	}

	std::vector<vk::Image> Swapchain::get_images() const
	{
		return parent()->getSwapchainImagesKHR(*this);
	}
}