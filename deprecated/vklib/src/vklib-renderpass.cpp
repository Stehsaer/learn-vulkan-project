#include "vklib-renderpass.hpp"

namespace vklib
{
	Result<Render_pass, VkResult> Render_pass::create(
		const Device&                      device,
		std::span<VkAttachmentDescription> attachments,
		std::span<VkSubpassDescription>    subpasses,
		std::span<VkSubpassDependency>     dependencies
	)
	{
		VkRenderPassCreateInfo create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
		create_info.attachmentCount        = attachments.size();
		create_info.pAttachments           = attachments.data();
		create_info.subpassCount           = subpasses.size();
		create_info.pSubpasses             = subpasses.data();
		create_info.dependencyCount        = dependencies.size();
		create_info.pDependencies          = dependencies.data();

		VkRenderPass pass;
		VkResult     result = vkCreateRenderPass(*device, &create_info, nullptr, &pass);

		if (result != VK_SUCCESS) return result;

		return Render_pass(pass, device);
	}

	void Render_pass::clean()
	{
		if (should_delete()) vkDestroyRenderPass(*parent, *content, parent.allocator_ptr());
	}
}