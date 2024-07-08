#include "binary-resource.hpp"
#include "logic.hpp"

std::shared_ptr<Application_logic_base> App_load_hdri_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);

	auto draw_logic = [&, this]() -> Application_logic_base*
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;
		}

		core->ui_controller.imgui_new_frame();

		const auto render_result = core->render_one_frame(
			[this](uint32_t idx)
			{
				draw(idx);
			}
		);

		if (!render_result) return nullptr;

		return this;
	};

	// render one frame before freeze
	if (draw_logic() == nullptr) return nullptr;
	if (draw_logic() == nullptr) return nullptr;

	core->env.device->waitIdle();

	// load HDRI
	try
	{
		core->env.log_msg("Loading HDRi from \"{}\"...", load_path);

		core->source.hdri = std::make_shared<Hdri_resource>();
		io::images::Stbi_image_utility hdri_image;

		// load image
		{
			auto hdri_command_buffer = core->env.command_buffer[0];

			hdri_command_buffer.begin();

			const auto staging_buffer
				= load_path == LOAD_DEFUALT_HDRI_TOKEN
					? hdri_image.load_hdri(core->env.allocator, hdri_command_buffer, binary_resource::builtin_hdr_span)
					: hdri_image.load_hdri(core->env.allocator, hdri_command_buffer, load_path);

			hdri_command_buffer.end();

			const auto submit_command_buffers = Command_buffer::to_array({hdri_command_buffer});
			core->env.t_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_command_buffers));

			core->env.device->waitIdle();
		}

		const Image_view hdri_view(
			core->env.device,
			hdri_image.image,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		core->source.hdri->generate(core->env, hdri_view, 1024, core->pipeline_set.lighting_pipeline.skybox_input_layout);
		core->source.hdri_path = load_path;

		load_state = Load_state::Load_success;
		core->env.log_msg("Loaded HDRi");
	}
	catch (const Exception& e)
	{
		load_fail_reason = std::format("{} at {}", e.msg, e.loc.function_name());
		load_state       = Load_state::Load_failed;

		core->env.log_err("Load HDRi failed, reason: {}", e.msg);

		// cleanup
		core->source.hdri = nullptr;
	}

	// After load
	while (true)
	{
		if (draw_logic() == nullptr) return nullptr;

		if (load_state == Load_state::Quit || load_state == Load_state::Load_success)
		{
			if (core->source.model != nullptr)
			{
				if (preset.has_value()) return std::make_shared<App_render_logic>(core, preset.value());

				return std::make_shared<App_render_logic>(core);
			}
			else
				return std::make_shared<App_idle_logic>(core);
		}
	}
}

void App_load_hdri_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_load_hdri_logic::ui_logic()
{
	ImGui::SetNextWindowPos(
		ImVec2(core->env.swapchain.extent.width / 2.0, core->env.swapchain.extent.height / 2.0),
		ImGuiCond_Always,
		{0.5, 0.5}
	);
	if (ImGui::Begin(
			"##Load Window",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		))
	{
		switch (load_state)
		{
		case Load_state::Start:
			ImGui::Text("Loading HDRI");
			ImGui::Text("Window may freeze, please wait.");
			break;
		case Load_state::Load_failed:
		{
			ImGui::Text("Load Failed:\n%s", load_fail_reason.c_str());
			if (ImGui::Button("OK")) load_state = Load_state::Quit;
			break;
		}
		case Load_state::Load_success:
		case Load_state::Quit:
			break;
		}
	}
	ImGui::End();

	ImGui::Render();
}
