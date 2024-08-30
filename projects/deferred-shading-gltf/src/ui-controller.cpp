#include "binary-resource.hpp"
#include "controller.hpp"

void Ui_controller::init_imgui(const Environment& env)
{
	// Descriptor Pool
	std::array<vk::DescriptorPoolSize, 1> pool_sizes;
	pool_sizes[0].setDescriptorCount(1).setType(vk::DescriptorType::eCombinedImageSampler);
	imgui_descriptor_pool = Descriptor_pool(env.device, pool_sizes, 1);

	// Renderpass
	{
		std::array<vk::AttachmentDescription, 1> attachment_descriptions;
		attachment_descriptions[0]
			.setFormat(env.swapchain.surface_format.format)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setSamples(vk::SampleCountFlagBits::e1);

		vk::AttachmentReference attachment_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

		std::array<vk::SubpassDescription, 1> subpasses;
		subpasses[0].setColorAttachments(attachment_ref).setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

		std::array<vk::SubpassDependency, 2> dependencies;
		dependencies[0]
			.setSrcSubpass(vk::SubpassExternal)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
		dependencies[1]
			.setSrcSubpass(0)
			.setDstSubpass(vk::SubpassExternal)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eAllGraphics)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

		imgui_renderpass = Render_pass(env.device, attachment_descriptions, subpasses, dependencies);

		attachment_descriptions[0].setInitialLayout(vk::ImageLayout::eUndefined).setLoadOp(vk::AttachmentLoadOp::eClear);
		imgui_unique_renderpass = Render_pass(env.device, attachment_descriptions, subpasses, dependencies);
	}

	// Framebuffers
	create_imgui_rt(env);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	imgui_window_initialized = ImGui_ImplSDL2_InitForVulkan(env.window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance                  = env.instance.to<VkInstance>();
	init_info.PhysicalDevice            = env.physical_device;
	init_info.Device                    = env.device.to<VkDevice>();
	init_info.QueueFamily               = env.g_family_idx;
	init_info.Queue                     = env.g_queue;
	init_info.PipelineCache             = nullptr;
	init_info.DescriptorPool            = imgui_descriptor_pool.to<VkDescriptorPool>();
	init_info.Subpass                   = 0;
	init_info.MinImageCount             = env.swapchain.image_count;
	init_info.ImageCount                = env.swapchain.image_count;
	init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator                 = nullptr;

	init_info.RenderPass     = imgui_renderpass.to<VkRenderPass>();
	imgui_vulkan_initialized = ImGui_ImplVulkan_Init(&init_info);

	if (!imgui_window_initialized || !imgui_vulkan_initialized) throw error::Detailed_error("Failed to initialize IMGUI!");

	float ddpi;
	SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(env.window), &ddpi, nullptr, nullptr);
	const float scale = ddpi / 96;

	if (scale > 4) throw error::Detailed_error(std::format("Abnormal Content DPI: {:.1f}dpi, {:.1f}x scale", ddpi, scale));

	std::vector<uint8_t> copied_font_data(binary_resource::roboto_font_data, binary_resource::roboto_font_data + binary_resource::roboto_font_size);

	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(copied_font_data.data(), copied_font_data.size(), 16 * scale, &font_cfg);
	io.Fonts->Build();

	ImGui::GetStyle().ScaleAllSizes(scale);
}

void Ui_controller::create_imgui_rt(const Environment& env)
{
	imgui_framebuffers.clear();
	for (auto i : Iota(env.swapchain.image_count))
	{
		imgui_framebuffers.emplace_back(Framebuffer(env.device, imgui_renderpass, {env.swapchain.image_views[i]}, vk::Extent3D(env.swapchain.extent, 1)));
	}
}

void Ui_controller::imgui_new_frame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void Ui_controller::imgui_draw(const Environment& env, uint32_t idx, bool unique)
{
	const auto& command_buffer = env.command_buffer[idx];

	const vk::ClearValue clear(vk::ClearColorValue(0, 0, 0, 0));

	command_buffer.begin_render_pass(
		unique ? imgui_unique_renderpass : imgui_renderpass,
		imgui_framebuffers[idx],
		vk::Rect2D({0, 0}, {env.swapchain.extent.width, env.swapchain.extent.height}),
		std::span(&clear, unique),
		vk::SubpassContents::eInline
	);

	ImDrawData* draw_data = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer.to<vk::CommandBuffer>());

	command_buffer.end_render_pass();
}

void Ui_controller::imgui_draw(const Environment& env, const Command_buffer& command_buffer, uint32_t idx, bool unique)
{
	const vk::ClearValue clear(vk::ClearColorValue(0, 0, 0, 0));

	command_buffer.begin_render_pass(
		unique ? imgui_unique_renderpass : imgui_renderpass,
		imgui_framebuffers[idx],
		vk::Rect2D({0, 0}, {env.swapchain.extent.width, env.swapchain.extent.height}),
		std::span(&clear, unique),
		vk::SubpassContents::eInline
	);

	ImDrawData* draw_data = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer.to<vk::CommandBuffer>());

	command_buffer.end_render_pass();
}

void Ui_controller::terminate_imgui() const
{
	if (imgui_vulkan_initialized) ImGui_ImplVulkan_Shutdown();

	if (imgui_window_initialized) ImGui_ImplSDL2_Shutdown();

	if (imgui_vulkan_initialized && imgui_window_initialized) ImGui::DestroyContext();
}