#pragma once

#include "core.hpp"
#include <barrier>
#include <condition_variable>
#include <semaphore>
#include <shared_mutex>

class Application_logic_base
{
  protected:

	std::shared_ptr<Core> core;

	Application_logic_base(std::shared_ptr<Core> core) :
		core(std::move(core))
	{
	}

  public:

	virtual ~Application_logic_base() { core->env.device->waitIdle(); }

	// Virtual work function, returns nullptr when terminating
	virtual std::shared_ptr<Application_logic_base> work() = 0;

	void test(const std::string& path)
	{
		const auto extension = std::filesystem::path(path).extension();
		core->params.model   = std::make_shared<io::mesh::gltf::Model>();

		io::mesh::gltf::Loader_context loader_context;
		loader_context.allocator                     = core->env.allocator;
		loader_context.command_pool                  = core->env.command_pool;
		loader_context.device                        = core->env.device;
		loader_context.transfer_queue                = core->env.t_queue;
		loader_context.physical_device               = core->env.physical_device;
		loader_context.texture_descriptor_set_layout = core->Pipeline_set.gbuffer_pipeline.descriptor_set_layout_texture;
		loader_context.albedo_only_layout            = core->Pipeline_set.shadow_pipeline.descriptor_set_layout_texture;

		if (extension == ".gltf")
			core->params.model->load_gltf_ascii(loader_context, path);
		else if (extension == ".glb")
			core->params.model->load_gltf_bin(loader_context, path);
	}
};

class App_render_logic : public Application_logic_base
{
  private:

	/* Statistics */

	std::atomic<double>   gbuffer_cpu_time, shadow_cpu_time;
	std::atomic<uint32_t> gbuffer_object_count, shadow_object_count, gbuffer_vertex_count, shadow_vertex_count;

	/* Render */

	std::array<Model_renderer, csm_count> shadow_renderer;
	Model_renderer                        gbuffer_renderer;

	Render_params::Draw_parameters draw_params;

	/* Multi-threading */

	bool                      multi_thread_stop = false;
	std::counting_semaphore<> frame_start_semaphore;

	void start_threads();
	void stop_threads();

	// barriers
	std::barrier<> model_rendering_statistic_barrier{csm_count + 2},  // (csm_count) Shadow Threads, 1 Gbuffer Thread, 1 Post Thread
		render_thread_barrier{csm_count + 3};  // (csm_count) Shadow Threads, 1 Gbuffer Thread, 1 Post Thread, 1 Main Thread

	// Current Image Index
	uint32_t current_idx = 0;

	// Threads
	std::array<std::jthread, csm_count> shadow_thread;
	std::jthread                        gbuffer_thread;
	std::jthread                        post_thread;

	// Vulkan Side Synchronization

	std::array<Command_buffer, csm_count> current_shadow_command_buffer;
	Command_buffer current_gbuffer_command_buffer, current_lighting_command_buffer, current_compute_command_buffer,
		current_composite_command_buffer;

	Semaphore deferred_semaphore, composite_semaphore, compute_semaphore, lighting_semaphore;

	void shadow_thread_work(uint32_t csm_idx);
	void gbuffer_thread_work();
	void post_thread_work();

	/* Draw Logic */

	void draw(uint32_t idx);
	void ui_logic();

	void draw_lighting(uint32_t idx, const Command_buffer& command_buffer);
	void compute_auto_exposure(uint32_t idx, const Command_buffer& command_buffer);
	void compute_bloom(uint32_t idx, const Command_buffer& command_buffer);
	void draw_composite(uint32_t idx, const Command_buffer& command_buffer);

	/* UI-related */
	bool show_panel = true, show_control = false, show_feature = false;

  public:

	virtual ~App_render_logic()
	{
		core->env.device->waitIdle();
		stop_threads();
	}

	App_render_logic(std::shared_ptr<Core> resource) :
		Application_logic_base(std::move(resource)),
		frame_start_semaphore(0)
	{
		deferred_semaphore  = Semaphore(core->env.device);
		composite_semaphore = Semaphore(core->env.device);
		compute_semaphore   = Semaphore(core->env.device);
		lighting_semaphore  = Semaphore(core->env.device);

		start_threads();
	}

	virtual std::shared_ptr<Application_logic_base> work();
};

class App_idle_logic : public Application_logic_base
{
  private:

	bool load_builtin_model = false, load_builtin_hdri = false;

  public:

	virtual ~App_idle_logic() {}

	App_idle_logic(std::shared_ptr<Core> resource) :
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

	App_load_model_logic(std::shared_ptr<Core> resource, std::string load_path) :
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

	App_load_hdri_logic(std::shared_ptr<Core> resource, std::string load_path) :
		Application_logic_base(std::move(resource)),
		load_path(std::move(load_path))
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();
};
