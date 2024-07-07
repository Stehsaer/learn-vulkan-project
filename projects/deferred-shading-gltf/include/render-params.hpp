#pragma once
#include "controller.hpp"
#include "environment.hpp"
#include "hdri.hpp"
#include "pipeline.hpp"

struct Render_source
{
	std::shared_ptr<io::mesh::gltf::Model> model;
	std::shared_ptr<Hdri_resource>         hdri;

	struct Material_data
	{
		Descriptor_set descriptor_set, albedo_only_descriptor_set;
	};

	Descriptor_pool            descriptor_pool;
	std::vector<Material_data> material_data;
	Buffer                     material_uniform_buffer;

	void generate_material_data(const Environment& env, const Pipeline_set& pipeline);
};

struct Camera_parameter
{
	glm::mat4 view_matrix, projection_matrix, view_projection_matrix, view_projection_matrix_inv;
	glm::vec3 eye_position, eye_direction;
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

	float bloom_start = 2.0, bloom_end = 15.0, bloom_intensity = 0.02;
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

	/*====== Generate ======*/

	glm::vec3                               get_light_direction() const;
	Camera_parameter                        get_gbuffer_parameter() const;
	std::array<Camera_parameter, csm_count> get_shadow_parameters() const;
	std::array<glm::vec2, csm_count>        get_shadow_sizes() const;

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
