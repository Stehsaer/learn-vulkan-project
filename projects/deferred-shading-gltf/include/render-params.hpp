#pragma once
#include "controller.hpp"
#include "environment.hpp"
#include "hdri.hpp"
#include "pipeline.hpp"

struct Render_source
{
	std::shared_ptr<io::mesh::gltf::Model> model;
	std::shared_ptr<Hdri_resource>         hdri;

	std::string model_path, hdri_path;

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
	algorithm::geometry::frustum::Frustum frustum;

	glm::mat4 view_matrix, projection_matrix, view_projection_matrix, view_projection_matrix_inv;
	glm::vec3 eye_position, eye_direction;

	Camera_parameter() = default;
	Camera_parameter(
		const algorithm::geometry::frustum::Frustum& frustum,
		const glm::mat4&                             view,
		const glm::mat4&                             projection,
		const glm::vec3&                             eye_position,
		const glm::vec3&                             eye_direction
	) :
		frustum(frustum),
		view_matrix(view),
		projection_matrix(projection),
		view_projection_matrix(projection * view),
		view_projection_matrix_inv(glm::inverse(view_projection_matrix)),
		eye_position(eye_position),
		eye_direction(eye_direction)
	{
	}
};

struct Shadow_parameter : public Camera_parameter
{
	glm::vec2 shadow_view_size;

	Shadow_parameter() = default;
	Shadow_parameter(
		const algorithm::geometry::frustum::Frustum& frustum,
		const glm::mat4&                             view,
		const glm::mat4&                             projection,
		const glm::vec3&                             eye_position,
		const glm::vec3&                             eye_direction,
		glm::vec2                                    shadow_view_size
	) :
		Camera_parameter(frustum, view, projection, eye_position, eye_direction),
		shadow_view_size(shadow_view_size)
	{
	}
};

struct Directional_light
{
	glm::vec3 color;
	float     intensity, yaw, pitch;
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

	Directional_light sun{
		{1.0, 1.0, 1.0},
		25.0,
		30.0,
		45.0
	};

	/*====== Generate ======*/

	glm::vec3        get_light_direction() const;
	Camera_parameter get_gbuffer_parameter(const Environment& env) const;

	Shadow_parameter get_shadow_parameters(
		float                   near,
		float                   far,
		float                   shadow_near,
		float                   shadow_far,
		const Camera_parameter& gbuffer_camera
	) const;

	std::array<glm::vec2, csm_count> get_shadow_sizes() const;

	inline float divide_projection_plane(float alpha) const
	{
		return glm::mix(glm::mix(near, far, alpha), near * glm::pow(far / near, alpha), csm_blend_factor);
	}
};
