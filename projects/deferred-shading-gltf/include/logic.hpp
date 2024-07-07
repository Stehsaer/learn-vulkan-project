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
};

class App_render_logic : public Application_logic_base
{
  private:

	/* Statistics */

	double    cpu_time;
	uint32_t  gbuffer_object_count, shadow_object_count, gbuffer_vertex_count, shadow_vertex_count;
	glm::vec3 scene_min_bound{0.0}, scene_max_bound{0.0};

	/* Render */

	Drawcall_generator                        gbuffer_generator;
	std::array<Drawcall_generator, csm_count> shadow_generator;

	std::array<Shadow_parameter, csm_count> shadow_params;
	Camera_parameter                        gbuffer_param;

	// Vulkan Objects

	struct Command_buffer_set
	{
		Command_buffer gbuffer_command_buffer, shadow_command_buffer, lighting_command_buffer, compute_command_buffer,
			composite_command_buffer;
	};

	std::vector<Command_buffer_set> command_buffers;

	Semaphore gbuffer_semaphore, shadow_semaphore, composite_semaphore, compute_semaphore, lighting_semaphore;

	/* Draw Logic */

	void draw(uint32_t idx);

	void draw_gbuffer(uint32_t idx, const Command_buffer& command_buffer);

	void update_uniforms(uint32_t idx);

	void draw_shadow(uint32_t idx, const Command_buffer& command_buffer);
	void draw_lighting(uint32_t idx, const Command_buffer& command_buffer);

	void compute_auto_exposure(uint32_t idx, const Command_buffer& command_buffer);
	void compute_bloom(uint32_t idx, const Command_buffer& command_buffer);
	void compute_process(uint32_t idx, const Command_buffer& command_buffer);

	void draw_composite(uint32_t idx, const Command_buffer& command_buffer);
	void draw_swapchain(uint32_t idx, const Command_buffer& command_buffer);

	void submit_commands(const Command_buffer_set& set);

	/* UI-related */

	void ui_logic();

	void stat_panel();
	void control_tab();
	void system_tab();
	void animation_tab();

	bool show_panel = true;

	std::unordered_map<uint32_t, vklib_hpp::io::mesh::gltf::Node_transformation> animation_buffer;

	int    selected_animation = -1;
	bool   animation_playing = false, animation_cycle = true;
	float  animation_time = 0.0, animation_rate = 1.0;
	double animation_start_time = 0.0;

	void update_animation();

  public:

	virtual ~App_render_logic() { core->env.device->waitIdle(); }

	App_render_logic(std::shared_ptr<Core> resource) :
		Application_logic_base(std::move(resource))
	{
		// Create semaphores
		gbuffer_semaphore   = Semaphore(core->env.device);
		shadow_semaphore    = Semaphore(core->env.device);
		composite_semaphore = Semaphore(core->env.device);
		compute_semaphore   = Semaphore(core->env.device);
		lighting_semaphore  = Semaphore(core->env.device);

		// Create command buffers
		command_buffers.resize(core->env.swapchain.image_count);
		for (auto& command_buffer : command_buffers)
		{
			command_buffer.shadow_command_buffer    = Command_buffer(core->env.command_pool);
			command_buffer.gbuffer_command_buffer   = Command_buffer(core->env.command_pool),
			command_buffer.lighting_command_buffer  = Command_buffer(core->env.command_pool),
			command_buffer.compute_command_buffer   = Command_buffer(core->env.command_pool),
			command_buffer.composite_command_buffer = Command_buffer(core->env.command_pool);
		}
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

	io::mesh::gltf::Load_stage load_stage   = io::mesh::gltf::Load_stage::Uninitialized;
	float                      sub_progress = 0.0;

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
	void load_thread_work();
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
