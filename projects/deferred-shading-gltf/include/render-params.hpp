#pragma once
#include "controller.hpp"
#include "environment.hpp"
#include "hdri.hpp"

struct Render_source
{
	std::shared_ptr<io::mesh::gltf::Model> model;
	std::shared_ptr<Hdri_resource>         hdri;
};

struct Render_params
{
	/*====== Cameras ======*/

	float             fov = 45, near = 0.01, far = 100.0;
	bool              auto_adjust_far_plane  = true;
	bool              auto_adjust_near_plane = true;
	Camera_controller camera_controller;

	/*====== Lighting & Exposure ======*/

	float exposure_ev         = 0;
	float emissive_brightness = 1;
	float skybox_brightness   = 1;

	float bloom_start = 2.0, bloom_end = 10.0, bloom_intensity = 0.03;
	float adapt_speed = 1;

	/*====== Shadow ======*/

	std::array<float, 3> shadow_near, shadow_far;
	float                csm_blend_factor = 0.5;

	/*====== Debug ======*/

	bool show_shadow_perspective  = false;
	int  shadow_perspective_layer = 0;

	/*====== Light Source ======*/

	glm::vec3    light_color     = glm::vec3(1.0);
	glm::float32 light_intensity = 20;
	float        sunlight_yaw = 0, sunlight_pitch = 45;

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

	struct Runtime_parameters
	{
		using Frustum = algorithm::geometry::frustum::Frustum;

		// Transformations
		glm::mat4                                          view_projection;
		std::array<glm::mat4, csm_count>                   shadow_transformations;

		// Frustums
		Frustum                        gbuffer_frustum;
		std::array<Frustum, csm_count> shadow_frustums;

		// Sizes
		std::array<glm::vec2, csm_count> shadow_view_sizes;

		glm::vec3 light_dir, light_color;  // Directional Light
		glm::vec3 eye_position, eye_path;  // Eye
	};

	Runtime_parameters gen_runtime_parameters(const Environment& env) const;
};
