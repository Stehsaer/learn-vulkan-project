#pragma once

#include "vklib-env.hpp"
#include "vklib-storage.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	class Swapchain : public Child_resource<vk::SwapchainKHR, Device>
	{
		using Child_resource<vk::SwapchainKHR, Device>::Child_resource;

		void clean() override;

	  public:

		Swapchain(
			const Device&                   device,
			const Surface&                  surface,
			const vk::SurfaceFormatKHR&     surface_format,
			const vk::PresentModeKHR&       present_mode,
			vk::Extent2D                    extent,
			uint32_t                        image_count,
			std::span<const uint32_t>       queue_families,
			vk::SurfaceTransformFlagBitsKHR transform,
			vk::SwapchainKHR                old_handle      = nullptr,
			bool                            clip            = true,
			vk::ImageUsageFlags             image_usage     = vk::ImageUsageFlagBits::eColorAttachment,
			vk::CompositeAlphaFlagBitsKHR   composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque
		);

		~Swapchain() override { clean(); }

		/* ====== Helpers ====== */

		std::vector<vk::Image> get_images() const;
	};
}