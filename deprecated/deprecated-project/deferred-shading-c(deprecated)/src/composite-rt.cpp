#include "application.hpp"

void Composite_render_target::create(const App_environment& env, const App_swapchain& swapchain)
{
	// Renderpass
	{
		VkAttachmentDescription swapchain_attachment_description{
			.format        = swapchain.format.format,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		auto attachments = std::to_array({swapchain_attachment_description});

		VkAttachmentReference swapchain_attachment_ref{
			.attachment = 0,
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &swapchain_attachment_ref
		};

		auto subpasses = std::to_array({subpass});

		VkSubpassDependency subpass_dependency{
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0,
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		};

		auto dependencies = std::to_array({subpass_dependency});

		auto result = Render_pass::create(env.device, attachments, subpasses, dependencies) >> renderpass;
		CHECK_RESULT(result, "Create Render Pass for Composite Pipeline Failed");
	}
}