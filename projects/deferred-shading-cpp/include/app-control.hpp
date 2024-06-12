#pragma once

#include "app-hdri.hpp"
#include "app-render.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

struct Camera_controller
{
	float     yaw = 0, pitch = 0, log_distance = 0;  // angles in degrees
	glm::vec3 eye_center = glm::vec3(0.0);

	float yaw_sensitivity = 0.05, pitch_sensitivity = 0.05, shift_x_sensitivity = 0.001, shift_y_sensitivity = 0.001,
		  zoom_sensitivity = 0.2;

	void update(ImGuiIO& io);

	glm::mat4 rotation_matrix() const;
	glm::mat4 view_matrix() const;
	glm::vec3 eye_position() const;
	glm::vec3 eye_path() const;
};

struct App_render_state
{

	/*====== Render Sources ======*/

	std::shared_ptr<io::mesh::gltf::Model> model;
	std::shared_ptr<Hdri_resource>         hdri;

	/*====== Acclerate Structures ======*/

	std::vector<std::tuple<uint32_t, uint32_t>> single_sided, double_sided;

	/*====== Cameras ======*/

	float             fov = 45, near = 0.01, far = 100.0;
	bool              auto_adjust_far_plane  = true;
	bool              auto_adjust_near_plane = true;
	Camera_controller camera_controller;

	float exposure_ev         = 0;
	float emissive_brightness = 1;
	float skybox_brightness   = 1;

	float bloom_start = 2.0, bloom_end = 5.0, bloom_intensity = 0.02;

	/* Shadow */
	std::array<float, 3> shadow_near, shadow_far;
	float                csm_blend_factor = 0.5;

	// Rendering Statistics
	double gbuffer_cpu_time, shadow_cpu_time;

	// Debug
	bool show_shadow_perspective  = false;
	int  shadow_perspective_layer = 0;

	/*====== Lighting ======*/

	glm::vec3    light_color     = glm::vec3(1.0);
	glm::float32 light_intensity = 20;
	float        sunlight_yaw = 0, sunlight_pitch = 45;
	float        adapt_speed = 1;

	inline glm::vec3 get_sunlight_direction() const
	{
		auto mat = glm::rotate(
			glm::rotate(glm::mat4(1.0), glm::radians<float>(sunlight_yaw), glm::vec3(0, 1, 0)),
			glm::radians<float>(sunlight_pitch),
			glm::vec3(0, 0, 1)
		);

		auto light = mat * glm::vec4(1, 0, 0, 0);
		return glm::normalize(glm::vec3(light));
	}

	/*====== Draw Parameters ======*/

	struct Draw_parameters
	{
		glm::mat4                                          view_projection, gbuffer_view_projection;
		algorithm::frustum_culling::Frustum                gbuffer_frustum;
		std::array<glm::mat4, 3>                           shadow_transformations;
		std::array<algorithm::frustum_culling::Frustum, 3> shadow_frustums;
		glm::vec3                                          light_dir, light_color, eye_position, eye_path;
		float                                              shadow_div_1, shadow_div_2;
	};

	Draw_parameters get_draw_parameters(const App_swapchain& swapchain) const;
};

struct Ui_controller
{
	Descriptor_pool          imgui_descriptor_pool;
	Render_pass              imgui_renderpass, imgui_unique_renderpass;
	std::vector<Framebuffer> imgui_framebuffers;

	// Debug
	bool show_demo_window = false;

	void init_imgui(const App_environment& env, const App_swapchain& swapchain);
	void create_imgui_rt(const App_environment& env, const App_swapchain& swapchain);
	void terminate_imgui() const;

	void imgui_new_frame();
	void imgui_draw(const App_environment& env, const App_swapchain& swapchain, uint32_t idx, bool unique = false);

	~Ui_controller() { terminate_imgui(); }

  private:

	bool imgui_window_initialized = false, imgui_vulkan_initialized = false;
};
