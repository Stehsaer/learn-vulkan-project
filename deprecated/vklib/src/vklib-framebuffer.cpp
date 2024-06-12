#include "vklib-framebuffer.hpp"

namespace vklib
{
	Result<Framebuffer, VkResult> Framebuffer::create(
		const Device&          device,
		const Render_pass&     render_pass,
		std::span<VkImageView> attachment_image_views,
		uint32_t               width,
		uint32_t               height,
		uint32_t               layer
	)
	{
		VkFramebufferCreateInfo create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		create_info.flags                   = 0;
		create_info.renderPass              = *render_pass;
		create_info.attachmentCount         = attachment_image_views.size();
		create_info.pAttachments            = attachment_image_views.data();
		create_info.width                   = width;
		create_info.height                  = height;
		create_info.layers                  = layer;

		VkFramebuffer handle;

		auto result = vkCreateFramebuffer(*device, &create_info, device.allocator_ptr(), &handle);
		if (result != VK_SUCCESS)
			return result;
		else
			return Framebuffer(handle, device);
	}

	void Framebuffer::clean()
	{
		if (should_delete()) vkDestroyFramebuffer(*parent, *content, parent.allocator_ptr());
	}
}