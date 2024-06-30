#pragma once

#include <atomic>
#include <filesystem>
#include <queue>
#include <thread>
#include <utility>
#include <vklib>

using namespace vklib_hpp;

#include "controller.hpp"
#include "environment.hpp"
#include "hdri.hpp"
#include "model-renderer.hpp"
#include "pipeline.hpp"
#include "render-params.hpp"
#include "render-target.hpp"

struct Core
{
	Environment env;

	Render_targets render_targets;
	Pipeline_set   Pipeline_set;
	Ui_controller  ui_controller;

	Render_source source;
	Render_params params;

	uint32_t command_buffer_idx = 0;

	/*======= Main Loop =======*/

	using loop_func = std::function<void(uint32_t idx)>;
	bool render_one_frame(const loop_func& func);
	void recreate_swapchain();

	Core()
	{
		initialize_sdl();

		env.create();
		Pipeline_set.create(env);
		render_targets.create(env, Pipeline_set);
		ui_controller.init_imgui(env);
	}
};
