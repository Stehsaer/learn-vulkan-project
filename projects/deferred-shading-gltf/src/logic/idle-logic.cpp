#include "logic.hpp"
#include <imgui_stdlib.h>

std::shared_ptr<Application_logic_base> App_idle_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);

	while (true)
	{
		// Poll SDL Events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;

			// File Dropped
			if (event.type == SDL_DROPFILE)
			{
				const auto file_path_str = std::string(event.drop.file);
				const auto path          = std::filesystem::path(file_path_str);
				const auto extension     = path.extension();

				if (!std::filesystem::is_regular_file(path))
				{
					continue;
				}

				if (extension == ".gltf" || extension == ".glb") return std::make_shared<App_load_model_logic>(core, file_path_str);

				if (extension == ".hdr") return std::make_shared<App_load_hdri_logic>(core, file_path_str);
			}

			if (load_builtin_model) return std::make_shared<App_load_model_logic>(core, LOAD_DEFAULT_MODEL_TOKEN);

			if (load_builtin_hdri) return std::make_shared<App_load_hdri_logic>(core, LOAD_DEFUALT_HDRI_TOKEN);

			if (load_preset) return std::make_shared<App_load_model_logic>(core, std::move(preset));
		}

		core->ui_controller.imgui_new_frame();

		core->render_one_frame(
			[this](uint32_t idx)
			{
				draw(idx);
			}
		);
	}
}

void App_idle_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_idle_logic::ui_logic()
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
		ImGui::Text("Drop File Here to Load!");
		ImGui::Separator();

		ImGui::BulletText("Model: %s", core->source.model == nullptr ? "Not Loaded" : "Loaded");
		ImGui::BulletText("HDRI: %s", core->source.hdri == nullptr ? "Not Loaded" : "Loaded");

		ImGui::Separator();

		if (ImGui::Button("Load Built-in Model")) load_builtin_model = true;
		ImGui::SameLine();
		if (ImGui::Button("Load Built-in HDRI")) load_builtin_hdri = true;

		ImGui::SeparatorText("Load Preset");

		ImGui::InputTextMultiline("JSON", &deserialize_buffer);
		if (ImGui::Button("Load"))
		{
			try
			{
				const auto json = nlohmann::json::parse(deserialize_buffer);
				preset.deserialize(json);
				load_preset = true;
			}
			catch (const std::exception& e)
			{
				deserialize_error_msg = e.what();
			}
		}

		if (!deserialize_error_msg.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, {1.0, 0.5, 0, 1.0});
			ImGui::TextWrapped("Error: %s", deserialize_error_msg.c_str());
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();

	ImGui::Render();
}
