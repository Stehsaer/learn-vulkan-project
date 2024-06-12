#include "application.hpp"

void Deferred_render_target::create_images(const App_environment& env, const App_swapchain& swapchain)
{
	// Cleanup
	{
		position.clear(), position_view.clear();
		normal.clear(), normal_view.clear();
		albedo.clear(), albedo_view.clear();
		pbr.clear(), pbr_view.clear();
	}

	// position (R32G32B32A32 SFlOAT)
	for (auto i : Range(swapchain.image_count))
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {swapchain.extent.width, swapchain.extent.height, 1},
						  VK_FORMAT_R32G32B32A32_SFLOAT,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> position.emplace_back();
		CHECK_RESULT(result, "Create Position Image for Deferred Rendering Failed");

		auto view_result = Image_view::create(
							   env.device,
							   position[i],
							   VK_FORMAT_R32G32B32A32_SFLOAT,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
						   )
			>> position_view.emplace_back();
		CHECK_RESULT(view_result, "Create Position Image View for Deferred Rendering Failed");
	}

	// normal (R32G32B32A32 SFLOAT)
	for (auto i : Range(swapchain.image_count))
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {swapchain.extent.width, swapchain.extent.height, 1},
						  VK_FORMAT_R32G32B32A32_SFLOAT,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> normal.emplace_back();
		CHECK_RESULT(result, "Create Normal Image for Deferred Rendering Failed");

		auto view_result = Image_view::create(
							   env.device,
							   normal[i],
							   VK_FORMAT_R32G32B32A32_SFLOAT,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
						   )
			>> normal_view.emplace_back();
		CHECK_RESULT(view_result, "Create Normal Image View for Deferred Rendering Failed");
	}

	// albedo (R8G8B8A8_UNORM)
	for (auto i : Range(swapchain.image_count))
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {swapchain.extent.width, swapchain.extent.height, 1},
						  VK_FORMAT_R8G8B8A8_UNORM,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> albedo.emplace_back();
		CHECK_RESULT(result, "Create Albedo Image for Deferred Rendering Failed");

		auto view_result = Image_view::create(
							   env.device,
							   albedo[i],
							   VK_FORMAT_R8G8B8A8_UNORM,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
						   )
			>> albedo_view.emplace_back();
		CHECK_RESULT(view_result, "Create Albedo Image View for Deferred Rendering Failed");
	}

	// pbr (R8G8B8A8_UNORM)
	for (auto i : Range(swapchain.image_count))
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {swapchain.extent.width, swapchain.extent.height, 1},
						  VK_FORMAT_R8G8B8A8_UNORM,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> pbr.emplace_back();
		CHECK_RESULT(result, "Create PBR Image for Deferred Rendering Failed");

		auto view_result = Image_view::create(
							   env.device,
							   albedo[i],
							   VK_FORMAT_R8G8B8A8_UNORM,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
						   )
			>> pbr_view.emplace_back();
		CHECK_RESULT(view_result, "Create PBR Image View for Deferred Rendering Failed");
	}

	// depth (D32)
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {swapchain.extent.width, swapchain.extent.height, 1},
						  VK_FORMAT_D32_SFLOAT,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> depth_buffer;
		CHECK_RESULT(result, "Create Depth Buffer Failed");

		auto view_result = Image_view::create(
							   env.device,
							   depth_buffer,
							   VK_FORMAT_D32_SFLOAT,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
						   )
			>> depth_view;
		CHECK_RESULT(view_result, "Create Depth Buffer View Failed");
	}
}

void Deferred_render_target::create_renderpass(const App_environment& env)
{
	/* ====== Attachment Descriptions ====== */

	VkAttachmentDescription deferred_pos_attachment{
		.format        = VK_FORMAT_R32G32B32A32_SFLOAT,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkAttachmentDescription deferred_normal_attachment{
		.format        = VK_FORMAT_R32G32B32A32_SFLOAT,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkAttachmentDescription deferred_color_attachment{
		.format        = VK_FORMAT_R8G8B8A8_UNORM,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkAttachmentDescription deferred_pbr_attachment{
		.format        = VK_FORMAT_R8G8B8A8_UNORM,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkAttachmentDescription deferred_depth_attachment{
		.format        = VK_FORMAT_D32_SFLOAT,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	auto attachment_descriptions = std::to_array(
		{deferred_pos_attachment,
		 deferred_normal_attachment,
		 deferred_color_attachment,
		 deferred_pbr_attachment,
		 deferred_depth_attachment}
	);

	/* ====== Subpasses ====== */

	auto deferred_pass_color_attachment_ref = std::to_array<VkAttachmentReference>({
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	});

	VkAttachmentReference deferred_pass_depth_attachment_ref = {4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription deferred_pass_description{
		.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount    = deferred_pass_color_attachment_ref.size(),
		.pColorAttachments       = deferred_pass_color_attachment_ref.data(),
		.pDepthStencilAttachment = &deferred_pass_depth_attachment_ref
	};

	auto subpasses = std::to_array({deferred_pass_description});

	/* ====== Subpass Dependencies ====== */

	VkSubpassDependency external_deferred_dependency{
		.srcSubpass    = VK_SUBPASS_EXTERNAL,
		.dstSubpass    = 0,
		.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};

	auto subpass_dependencies = std::to_array({external_deferred_dependency});

	auto result
		= Render_pass::create(env.device, attachment_descriptions, subpasses, subpass_dependencies) >> renderpass;
	CHECK_RESULT(result, "Create Render Pass Failed");
}

void Deferred_render_target::create_framebuffers(const App_environment& env, const App_swapchain& swapchain)
{
	// 0 -> position
	// 1 -> normal
	// 2 -> color
	// 3 -> pbr
	// 4 -> depth

	framebuffer.clear();

	for (auto i : Range(swapchain.image_count))
	{
		auto image_views
			= std::to_array({*position_view[i], *normal_view[i], *albedo_view[i], *pbr_view[i], *depth_view});

		auto result
			= Framebuffer::create(env.device, renderpass, image_views, swapchain.extent.width, swapchain.extent.height)
			>> framebuffer.emplace_back();
		CHECK_RESULT(result, "Create Deferred Framebuffer Failed");
	}
}

void Deferred_render_target::create(const App_environment& env, const App_swapchain& swapchain)
{
	create_images(env, swapchain);
	create_renderpass(env);
	create_framebuffers(env, swapchain);
}
