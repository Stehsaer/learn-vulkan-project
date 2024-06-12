#pragma once

#include "vklib-device.hpp"
#include "vklib-image.hpp"
#include "vklib-renderpass.hpp"

namespace vklib
{
	class Framebuffer : public Base_copyable_parented<VkFramebuffer, Device>
	{
		using Base_copyable_parented<VkFramebuffer, Device>::Base_copyable_parented;

	  public:

		static Result<Framebuffer, VkResult> create(
			const Device&          device,
			const Render_pass&     render_pass,
			std::span<VkImageView> attachment_image_views,
			uint32_t               width,
			uint32_t               height,
			uint32_t               layer = 1
		);

		void clean() override;
		~Framebuffer() override { clean(); };
	};
}