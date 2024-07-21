#include "binary-resource.hpp"
#include "logic.hpp"

void App_load_model_logic::load_thread_work()
{
	core->env.log_msg("Loading model from \"{}\"...", load_path);
	core->source.model = std::make_shared<io::gltf::Model>();  // create new

	// Loader Context
	io::gltf::Loader_context loader_context;

	loader_context.allocator               = core->env.allocator;
	loader_context.command_pool            = Command_pool(core->env.device, core->env.g_family_idx);
	loader_context.device                  = core->env.device;
	loader_context.transfer_queue          = core->env.t_queue;
	loader_context.physical_device         = core->env.physical_device;
	loader_context.load_stage              = &load_stage;
	loader_context.sub_progress            = &sub_progress;
	loader_context.config.enable_anistropy = core->env.features.anistropy_enabled;
	loader_context.config.max_anistropy    = std::min(8.0f, core->env.features.max_anistropy);

	const auto extension = std::filesystem::path(load_path).extension();

	try
	{
		if (load_path == LOAD_DEFAULT_MODEL_TOKEN)
		{
			core->source.model->load_gltf_memory(
				loader_context,
				std::span((uint8_t*)binary_resource::damaged_helmet_data, binary_resource::damaged_helmet_size)
			);
		}
		else
		{
			if (extension == ".gltf")
				core->source.model->load_gltf_ascii(loader_context, load_path);
			else
				core->source.model->load_gltf_bin(loader_context, load_path);
		}

		core->source.model_path = load_path;
		core->source.generate_material_data(core->env, core->pipeline_set);
		core->source.generate_skin_data(core->env, core->pipeline_set);

		core->env.log_msg("Loaded model");
	}
	catch (const Exception& e)
	{
		load_err_msg       = e.msg;
		load_stage         = io::gltf::Load_stage::Error;
		core->source.model = nullptr;

		core->env.log_err("Load model failed, reason: {}", e.msg);
		return;
	}
}

std::shared_ptr<Application_logic_base> App_load_model_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);

	load_thread = std::jthread(
		[this]
		{
			this->load_thread_work();
		}
	);

	while (true)
	{
		// Poll SDL Events
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

		if (quit)
		{
			if (preset.has_value()) return std::make_shared<App_load_hdri_logic>(core, std::move(preset.value()));

			if (load_stage == io::gltf::Load_stage::Finished && core->source.hdri != nullptr)
			{
				return std::make_shared<App_render_logic>(core);
			}
			else
				return std::make_shared<App_idle_logic>(core);
		}
	}
}

void App_load_model_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_load_model_logic::ui_logic()
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
		const char* display_msg      = "";
		float       display_progress = sub_progress;

		switch (load_stage)
		{
		case vklib::io::gltf::Load_stage::Uninitialized:
			break;
		case vklib::io::gltf::Load_stage::Tinygltf_loading:
			display_msg      = "TinyGltf Loading Data";
			display_progress = -1;
			break;
		case vklib::io::gltf::Load_stage::Load_material:
			display_msg = "Loading Material";
			break;
		case vklib::io::gltf::Load_stage::Load_mesh:
			display_msg = "Loading Mesh";
			break;
		case vklib::io::gltf::Load_stage::Finished:
			break;
		case vklib::io::gltf::Load_stage::Error:
			display_msg = "Error!";
			break;
		}

		ImGui::Text("%s", display_msg);

		if (load_stage == vklib::io::gltf::Load_stage::Error)
		{
			ImGui::BulletText("%s", load_err_msg.c_str());
			if (ImGui::Button("OK")) quit = true;
		}
		else if (load_stage == vklib::io::gltf::Load_stage::Finished)
		{
			quit = true;
		}
		else
		{
			// Show Progress bar
			if (display_progress >= 0)
				ImGui::ProgressBar(display_progress, ImVec2(core->env.swapchain.extent.width / 3.0f, 0));
			else
				ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(core->env.swapchain.extent.width / 3.0f, 0));
		}
	}
	ImGui::End();

	ImGui::Render();
}
