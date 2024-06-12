#pragma once

#include <atomic>
#include <filesystem>
#include <queue>
#include <thread>
#include <utility>
#include <vklib>

using namespace vklib_hpp;

#include "app-basic.hpp"
#include "app-control.hpp"
#include "app-hdri.hpp"
#include "app-render.hpp"

struct App_resource
{
	App_environment env;
	App_swapchain   swapchain;

	Main_rt       main_rt;
	Main_pipeline main_pipeline;
	Ui_controller ui_controller;

	App_render_state render_state;
	Model_renderer   model_renderer;

	uint32_t command_buffer_idx = 0;

	/*======= Main Loop =======*/

	using loop_func = std::function<void(uint32_t idx)>;
	bool render_one_frame(const loop_func& func);
	void recreate_swapchain();

	App_resource()
	{
		initialize_sdl();

		env.create();
		swapchain.create(env);
		main_pipeline.create(env, swapchain);
		main_rt.create(env, swapchain, main_pipeline);
		ui_controller.init_imgui(env, swapchain);
	}
};

class Application_logic_base
{
  protected:

	std::shared_ptr<App_resource> resource;

	Application_logic_base(std::shared_ptr<App_resource> resource) :
		resource(std::move(resource))
	{
	}

  public:

	virtual ~Application_logic_base() { resource->env.device->waitIdle(); }

	// Virtual work function, returns nullptr when terminating
	virtual std::shared_ptr<Application_logic_base> work() = 0;

	void test(const std::string& path)
	{
		const auto extension         = std::filesystem::path(path).extension();
		resource->render_state.model = std::make_shared<io::mesh::gltf::Model>();

		io::mesh::gltf::Loader_context loader_context;
		loader_context.allocator       = resource->env.allocator;
		loader_context.command_pool    = resource->env.command_pool;
		loader_context.device          = resource->env.device;
		loader_context.transfer_queue  = resource->env.t_queue;
		loader_context.physical_device = resource->env.physical_device;
		loader_context.texture_descriptor_set_layout
			= resource->main_pipeline.gbuffer_pipeline.descriptor_set_layout_texture;
		loader_context.albedo_only_layout = resource->main_pipeline.shadow_pipeline.descriptor_set_layout_texture;

		if (extension == ".gltf")
			resource->render_state.model->load_gltf_ascii(loader_context, path);
		else if (extension == ".glb")
			resource->render_state.model->load_gltf_bin(loader_context, path);
	}
};

class App_render_logic : public Application_logic_base
{
  public:

	virtual ~App_render_logic() {}

	App_render_logic(std::shared_ptr<App_resource> resource) :
		Application_logic_base(std::move(resource))
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();

	void draw_shadow(uint32_t idx, const App_render_state::Draw_parameters& draw_params);
	void draw_gbuffer(uint32_t idx, const App_render_state::Draw_parameters& draw_params);
	void draw_lighting(uint32_t idx);
	void compute_auto_exposure(uint32_t idx);
	void compute_bloom(uint32_t idx);
	void draw_composite(uint32_t idx);
};

class App_idle_logic : public Application_logic_base
{
  private:

	bool load_builtin_model = false, load_builtin_hdri = false;

  public:

	virtual ~App_idle_logic() {}

	App_idle_logic(std::shared_ptr<App_resource> resource) :
		Application_logic_base(std::move(resource))
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();
};

class App_load_model_logic : public Application_logic_base
{
  private:

	std::string  load_path;
	std::jthread load_thread;

	std::atomic<io::mesh::gltf::Load_stage> load_stage = io::mesh::gltf::Load_stage::Uninitialized;
	std::string                             load_err_msg;

	bool quit = false;  // set to true when quitting the state

  public:

	App_load_model_logic(std::shared_ptr<App_resource> resource, std::string load_path) :
		Application_logic_base(std::move(resource)),
		load_path(std::move(load_path))
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();
};

class App_load_hdri_logic : public Application_logic_base
{
  private:

	enum class Load_state
	{
		Start,
		Load_failed,
		Load_success,
		Quit
	} load_state = Load_state::Start;

	std::string load_fail_reason;

	std::string load_path;

  public:

	App_load_hdri_logic(std::shared_ptr<App_resource> resource, std::string load_path) :
		Application_logic_base(std::move(resource)),
		load_path(std::move(load_path))
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();
};
