#pragma once

#include "vklib-base.hpp"
#include "vklib-common.h"
#include "vklib-device.hpp"
#include "vklib-image.hpp"
#include "vklib-synchronize.hpp"

namespace vklib
{
	class Swapchain : public Base_copyable_parented<VkSwapchainKHR, Device>
	{
		using Base_copyable_parented<VkSwapchainKHR, Device>::Base_copyable_parented;

	  public:

		static Result<Swapchain, VkResult> create(
			const Device&                 device,
			const Surface&                surface,
			const VkSurfaceFormatKHR&     surface_format,
			const VkPresentModeKHR&       present_mode,
			VkExtent2D                    extent,
			uint32_t                      image_count,
			std::span<uint32_t>           queue_families,
			VkSurfaceTransformFlagBitsKHR transform,
			/* OPTIONAL */
			VkSwapchainKHR              old_handle      = VK_NULL_HANDLE,
			bool                        clip            = true,
			VkImageUsageFlags           image_usage     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		);

		void clean() override;
		~Swapchain() override { clean(); }

		std::vector<Image> get_images() const;

		Result<uint32_t, VkResult> acquire_next_image(
			const std::optional<Semaphore>& signal_semaphore,
			const std::optional<Fence>&     signal_fence,
			uint64_t                        timeout = std::numeric_limits<uint64_t>::max()
		);
	};
}