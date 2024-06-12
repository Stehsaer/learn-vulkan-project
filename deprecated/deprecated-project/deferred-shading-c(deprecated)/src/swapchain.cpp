#include "application.hpp"

VkSurfaceFormatKHR App_swapchain::select_swapchain_format(const Swapchain_detail& swapchain_detail)
{
	// No format
	if (swapchain_detail.formats.size() == 1 && swapchain_detail.formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_R8G8B8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	// Select format from video mode
	VkFormat target_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

	// Search for target format
	for (const auto& format : swapchain_detail.formats)
		if (format.format == target_format && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;

	// fallback: search for RGB8
	for (const auto& format : swapchain_detail.formats)
		if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;

	return swapchain_detail.formats[0];
}

void App_swapchain::create(const App_environment& env)
{
	// Create swapchain
	{
		auto swapchain_detail = env.physical_device.get_swapchain_detail(env.surface);

		// choose swapchain format
		format = select_swapchain_format(swapchain_detail);

		// choose present mode
		VkPresentModeKHR present_mode = [=]() -> VkPresentModeKHR
		{
			// first priority: triple buffer
			for (const auto& mode : swapchain_detail.present_modes)
				if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;

			// fallback: immediate
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
		}();

		// get swap extent
		int width, height;
		glfwGetFramebufferSize(*env.window, &width, &height);
		extent.width = width, extent.height = height;

		// get swap image count
		image_count = swapchain_detail.capabilities.minImageCount + 1;
		if (swapchain_detail.capabilities.maxImageCount > 0)
			image_count = std::min(image_count, swapchain_detail.capabilities.maxImageCount);

		// queue families
		auto queue_families    = std::to_array({env.g_queue_family, env.p_queue_family});
		auto queue_family_span = std::span(queue_families.begin(), env.g_queue_family == env.p_queue_family ? 1 : 2);

		// create swapchain
		auto result = Swapchain::create(
						  env.device,
						  env.surface,
						  format,
						  present_mode,
						  extent,
						  image_count,
						  queue_family_span,
						  swapchain_detail.capabilities.currentTransform,
						  *handle
					  )
			>> handle;
		CHECK_RESULT(result, "Create Swapchain");
	}

	// Get swapchain images & image views
	{
		// Image
		images = handle.get_images();
		image_views.clear();  // clear original

		// Image views
		for (uint32_t i = 0; i < image_count; i++)
		{
			auto result = Image_view::create(
							  env.device,
							  images[i],
							  format.format,
							  utility_values::default_component_mapping,
							  VK_IMAGE_VIEW_TYPE_2D,
							  VkImageSubresourceRange{
								  .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
								  .baseMipLevel   = 0,
								  .levelCount     = 1,
								  .baseArrayLayer = 0,
								  .layerCount     = 1
							  }
						  )
				>> image_views.emplace_back();
			CHECK_RESULT(result, "Create Swapchain Image Views");
		}
	}

	// Create Depth Buffer
	{
		// create & allocate image for depth buffer
		auto depth_buffer_result = Image::create(
									   env.allocator,
									   VK_IMAGE_TYPE_2D,
									   {extent.width, extent.height, 1},
									   VK_FORMAT_D32_SFLOAT,
									   VK_IMAGE_TILING_OPTIMAL,
									   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
									   VMA_MEMORY_USAGE_GPU_ONLY,
									   VK_SHARING_MODE_EXCLUSIVE
								   )
			>> depth_buffer;
		CHECK_RESULT(depth_buffer_result, "Create Depth Buffer");

		// create image view for depth buffer
		auto depth_buffer_view_result = Image_view::create(
											env.device,
											depth_buffer,
											VK_FORMAT_D32_SFLOAT,
											utility_values::default_component_mapping,
											VK_IMAGE_VIEW_TYPE_2D,
											VkImageSubresourceRange{
												.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
												.baseMipLevel   = 0,
												.levelCount     = 1,
												.baseArrayLayer = 0,
												.layerCount     = 1
											}
										)
			>> depth_buffer_view;
		CHECK_RESULT(depth_buffer_view_result, "Create Depth Buffer View");
	}
}