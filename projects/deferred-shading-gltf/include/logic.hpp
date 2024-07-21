#pragma once

#include "core.hpp"
#include <nlohmann/json.hpp>
#include <utility>

#define LOAD_DEFAULT_MODEL_TOKEN "**cmd-load-built-in--model**"
#define LOAD_DEFUALT_HDRI_TOKEN "**cmd-load-built-in--hdri**"

class Application_logic_base
{
  protected:

	Application_logic_base(std::shared_ptr<Core> core) :
		core(std::move(core))
	{
	}

  public:

	std::shared_ptr<Core> core;

	virtual ~Application_logic_base() { core->env.device->waitIdle(); }

	// Virtual work function, returns nullptr when terminating
	virtual std::shared_ptr<Application_logic_base> work() = 0;
};

// Preset for debugging, essential for debugging in same scenes
struct Scene_preset
{
	/* Path */

	std::string hdri_path, model_path;

	/* Camera */

	float     camera_yaw, camera_pitch, camera_log_distance, camera_fov;  // angles in degrees
	glm::vec3 camera_eye_center;

	/* Parameters */

	float exposure_ev;
	float emissive_brightness;
	float skybox_brightness;

	float bloom_start, bloom_end, bloom_intensity;
	float adapt_speed;
	float csm_blend_factor;
	float fov;

	glm::vec3 light_color;
	float     light_intensity;
	float     sunlight_yaw, sunlight_pitch;

	void collect(const Application_logic_base& src);
	void collect(const Render_params& src);

	nlohmann::json serialize() const;

	void deserialize(const nlohmann::json& json);

	void assign(Application_logic_base& dst) const;
	void assign(Render_params& dst) const;
};

class App_render_logic : public Application_logic_base
{
  private:

	/* Load Logic */
	std::string deserialize_buffer, deserialize_error_msg;

	Scene_preset load_preset_src;
	bool         load_preset = false, load_default_hdri = false, load_default_model = false;

	/* Statistics */

	double    cpu_time;
	uint32_t  gbuffer_object_count, shadow_object_count, gbuffer_vertex_count, shadow_vertex_count;
	glm::vec3 scene_min_bound{0.0}, scene_max_bound{0.0};

	/* Generator */

	Drawcall_generator                        gbuffer_generator;
	std::array<Drawcall_generator, csm_count> shadow_generator;

	std::array<Shadow_parameter, csm_count> shadow_params;
	Camera_parameter                        gbuffer_param;

	Node_traverser traverser;

	// Vulkan Objects

	struct Command_buffer_set
	{
		Command_buffer animation_update_command_buffer, gbuffer_command_buffer, shadow_command_buffer, lighting_command_buffer,
			compute_command_buffer, composite_command_buffer;

		Command_buffer_set(const Command_pool& pool)
		{
			animation_update_command_buffer = {pool};
			gbuffer_command_buffer          = {pool};
			shadow_command_buffer           = {pool};
			lighting_command_buffer         = {pool};
			compute_command_buffer          = {pool};
			composite_command_buffer        = {pool};
		}
	};

	std::vector<Command_buffer_set> command_buffers;

	Semaphore copy_buffer_semaphore, gbuffer_shadow_semaphore, composite_semaphore, compute_semaphore, lighting_semaphore;

	/* Draw Logic */

	// Draw
	void draw(uint32_t idx);

	void generate_drawcalls(uint32_t idx);
	void update_uniforms(uint32_t idx);

	void draw_gbuffer(uint32_t idx, const Command_buffer& command_buffer);
	void draw_shadow(uint32_t idx, const Command_buffer& command_buffer);
	void draw_lighting(uint32_t idx, const Command_buffer& command_buffer);

	void compute_auto_exposure(uint32_t idx, const Command_buffer& command_buffer);
	void compute_bloom(uint32_t idx, const Command_buffer& command_buffer);
	void compute_process(uint32_t idx, const Command_buffer& command_buffer);

	void draw_composite(uint32_t idx, const Command_buffer& command_buffer);
	void draw_ui(uint32_t idx, const Command_buffer& command_buffer);
	void execute_fxaa(uint32_t idx, const Command_buffer& command_buffer);
	void draw_swapchain(uint32_t idx, const Command_buffer& command_buffer);

	// Submit commands to queues
	void submit_commands(const Command_buffer_set& set) const;

	/* UI-related */

	void ui_logic();  // All ui logic goes here

	void stat_panel();     // Real-time stats panel
	void lighting_tab();   // Control panel
	void system_tab();     // System panel
	void animation_tab();  // Animation panel, only activate if animation present
	void preset_tab();     // Debug panel
	void camera_tab();     // Camera panel

	bool show_panel = true;

	std::unordered_map<uint32_t, vklib::io::gltf::Node_transformation> animation_buffer;

	int    selected_animation = -1;
	bool   animation_playing = false, animation_cycle = true;
	float  animation_time = 0.0, animation_rate = 1.0;
	double animation_start_time = 0.0;

	void update_animation();  // Update animation
	void upload_skin(uint32_t idx);

	int selected_camera = -1;

	std::string exported_preset_json;

  public:

	virtual ~App_render_logic() { core->env.device->waitIdle(); }

	App_render_logic(std::shared_ptr<Core> resource);

	App_render_logic(std::shared_ptr<Core> resource, const Scene_preset& preset);

	virtual std::shared_ptr<Application_logic_base> work();
};

class App_idle_logic : public Application_logic_base
{
  private:

	bool load_builtin_model = false, load_builtin_hdri = false, load_preset = false;

	Scene_preset preset;
	std::string  deserialize_error_msg, deserialize_buffer;

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

	std::optional<Scene_preset> preset = std::nullopt;

	std::string  load_path;
	std::jthread load_thread;

	io::gltf::Load_stage       load_stage   = io::gltf::Load_stage::Uninitialized;
	float                      sub_progress = 0.0;

	std::string                             load_err_msg;

	bool quit = false;  // set to true when quitting the state

  public:

	App_load_model_logic(std::shared_ptr<Core> resource, std::string load_path) :
		Application_logic_base(std::move(resource)),
		load_path(std::move(load_path))
	{
	}

	App_load_model_logic(std::shared_ptr<Core> resource, Scene_preset&& preset) :
		Application_logic_base(std::move(resource)),
		preset(std::move(preset)),
		load_path(this->preset.value().model_path)
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

	std::optional<Scene_preset> preset = std::nullopt;

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

	App_load_hdri_logic(std::shared_ptr<Core> resource, Scene_preset&& preset) :
		Application_logic_base(std::move(resource)),
		preset(std::move(preset)),
		load_path(this->preset.value().hdri_path)
	{
	}

	virtual std::shared_ptr<Application_logic_base> work();

	void draw(uint32_t idx);
	void ui_logic();
};
