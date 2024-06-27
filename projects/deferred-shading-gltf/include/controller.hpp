#pragma once

#include "pipeline.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

struct Camera_controller
{
	float     target_yaw = 0, target_pitch = 0, target_log_distance = 0;  // angles in degrees
	glm::vec3 target_eye_center = glm::vec3(0.0);

	float     current_yaw = 0, current_pitch = 0, current_log_distance = 0;  // angles in degrees
	glm::vec3 current_eye_center = glm::vec3(0.0);

	float yaw_sensitivity = 0.05, pitch_sensitivity = 0.05, shift_x_sensitivity = 0.001, shift_y_sensitivity = 0.001,
		  zoom_sensitivity = 0.2, lerp_speed = 10;

	void update(ImGuiIO& io);

	glm::mat4 rotation_matrix() const;
	glm::mat4 view_matrix() const;
	glm::vec3 eye_position() const;
	glm::vec3 eye_path() const;
};

struct Ui_controller
{
	Descriptor_pool          imgui_descriptor_pool;
	Render_pass              imgui_renderpass, imgui_unique_renderpass;
	std::vector<Framebuffer> imgui_framebuffers;

	// Debug
	bool show_demo_window = false;

	void init_imgui(const Environment& env);
	void create_imgui_rt(const Environment& env);
	void terminate_imgui() const;

	void imgui_new_frame();
	void imgui_draw(const Environment& env, uint32_t idx, bool unique = false);
	void imgui_draw(const Environment& env, const Command_buffer& command_buffer, uint32_t idx, bool unique = false);

	~Ui_controller() { terminate_imgui(); }

  private:

	bool imgui_window_initialized = false, imgui_vulkan_initialized = false;
};
