#pragma once

#include "environment.hpp"

struct General_model_matrix
{
	glm::mat4 matrix;
};

struct Shadow_pipeline
{
	static constexpr vk::Format shadow_map_format = vk::Format::eD32Sfloat;

	// At Shadow Vert, set = 0
	struct Shadow_uniform
	{
		glm::mat4 shadow_matrix;
	};

	// At Shadow Vert, push_constant
	using Model_matrix = General_model_matrix;

	Descriptor_set_layout descriptor_set_layout_shadow_matrix,  // @vert, set = 0
		descriptor_set_layout_texture;                          // @frag, set = 1

	Pipeline_layout   pipeline_layout;
	Graphics_pipeline single_sided_pipeline, single_sided_pipeline_alpha, single_sided_pipeline_blend, double_sided_pipeline,
		double_sided_pipeline_alpha, double_sided_pipeline_blend;
	Render_pass       render_pass;

	static vk::ClearValue clear_value;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer, csm_count}
	});

	void create(const Environment& env);
};

struct Gbuffer_pipeline
{
	static constexpr vk::Format normal_format = vk::Format::eR32G32B32A32Sfloat, color_format = vk::Format::eR8G8B8A8Unorm,
								pbr_format = vk::Format::eR8G8B8A8Unorm, emissive_format = vk::Format::eR8G8B8A8Unorm,
								depth_format = vk::Format::eD32Sfloat;

	// At Gbuffer Vert, set = 0
	struct Camera_uniform
	{
		glm::mat4 view_projection_matrix;
	};

	// At Gbuffer Vert, push_constant
	using Model_matrix = General_model_matrix;

	Descriptor_set_layout descriptor_set_layout_texture,  // @frag, set = 1
		descriptor_set_layout_camera;                     // @vert, set = 0, binding = 0

	Pipeline_layout   pipeline_layout;
	Graphics_pipeline single_sided_pipeline, single_sided_pipeline_alpha, single_sided_pipeline_blend, double_sided_pipeline,
		double_sided_pipeline_alpha, double_sided_pipeline_blend;
	Render_pass       render_pass;

	static std::array<vk::ClearValue, 5> clear_values;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer,        2},
		{vk::DescriptorType::eCombinedImageSampler, 5}
	});

	void create(const Environment& env);
};

struct Lighting_pipeline
{
	static constexpr vk::Format luminance_format = vk::Format::eR16G16B16A16Sfloat;

	// At Lighting Frag, push_constant
	struct Params
	{
		alignas(16) glm::mat4 view_projection_inv;
		alignas(16) glm::mat4 shadow[3];

		alignas(16) struct
		{
			glm::vec2 texel_size;  // uv size of each texel
			glm::vec2 view_size;   // size of the shadow view
		} shadow_size[3];

		alignas(16) glm::vec3 camera_position;
		alignas(16) glm::vec3 sunlight_pos;
		alignas(16) glm::vec3 sunlight_color;
		alignas(4) float emissive_brightness = 1;
		alignas(4) float skybox_brightness   = 1;
	};

	// Layouts
	Descriptor_set_layout gbuffer_input_layout, skybox_input_layout;
	Pipeline_layout       pipeline_layout;
	Graphics_pipeline     pipeline;
	Render_pass           render_pass;

	static std::array<vk::ClearValue, 2> clear_value;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer,        1            },
		{vk::DescriptorType::eCombinedImageSampler, csm_count + 8}
	});

	void create(const Environment& env);
};

struct Auto_exposure_compute_pipeline
{
	static constexpr inline glm::uvec2 luminance_avg_workgroup_size{16, 16};
	static constexpr inline double     integrate_result = 0.2984349184369049;
	static constexpr inline float      min_luminance = -6, max_luminance = 15;

	// At luminance avg pipeline, set = 0, binding = 1, storage buffer
	struct Exposure_result
	{
		alignas(4) float luminance;
		alignas(4) float prev_luminance;
	};

	struct Exposure_medium
	{
		int32_t pixel_count[256];
	};

	// At lerp pipeline, push_constant
	struct Lerp_params
	{
		float adapt_speed;
		float delta_time;

		float min_luminance;
		float max_luminance;

		uint32_t texture_size_x;
		uint32_t texture_size_y;
	};

	struct Luminance_params
	{
		float min_luminance;
		float max_luminance;
	};

	Descriptor_set_layout luminance_avg_descriptor_set_layout;
	Pipeline_layout       luminance_avg_pipeline_layout;
	Compute_pipeline      luminance_avg_pipeline;

	Descriptor_set_layout lerp_descriptor_set_layout;
	Pipeline_layout       lerp_pipeline_layout;
	Compute_pipeline      lerp_pipeline;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eStorageImage,  1},
		{vk::DescriptorType::eStorageBuffer, 1}
	});

	inline static auto unvaried_descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eStorageBuffer, 2}
	});

	void create(const Environment& env);
};

struct Bloom_pipeline
{
	using Exposure_result = Auto_exposure_compute_pipeline::Exposure_result;

	inline static constexpr auto chain_format = Lighting_pipeline::luminance_format;

	struct Params
	{
		alignas(4) float start_threshold, end_threshold, exposure;
	};

	// Bloom filter pipeline

	Descriptor_set_layout bloom_filter_descriptor_set_layout;
	Pipeline_layout       bloom_filter_pipeline_layout;
	Compute_pipeline      bloom_filter_pipeline;

	Descriptor_set_layout bloom_blur_descriptor_set_layout;
	Pipeline_layout       bloom_blur_pipeline_layout;
	Compute_pipeline      bloom_blur_pipeline;

	Descriptor_set_layout bloom_acc_descriptor_set_layout;
	Pipeline_layout       bloom_acc_pipeline_layout;
	Compute_pipeline      bloom_acc_pipeline;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eStorageImage,         6},
		{vk::DescriptorType::eUniformBuffer,        1},
		{vk::DescriptorType::eCombinedImageSampler, 1}
	});

	void create(const Environment& env);
};

struct Composite_pipeline
{
	// At Composite Frag, set = 0, binding = 1
	struct Exposure_param
	{
		float exposure, bloom_strength;
	};

	// Layouts
	Descriptor_set_layout descriptor_set_layout;
	Pipeline_layout       pipeline_layout;
	Graphics_pipeline     pipeline;
	Render_pass           render_pass;

	static vk::ClearValue clear_value;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer,        2},
		{vk::DescriptorType::eCombinedImageSampler, 1}
	});

	void create(const Environment& env);
};

struct Pipeline_set
{
	Shadow_pipeline                shadow_pipeline;
	Gbuffer_pipeline               gbuffer_pipeline;
	Lighting_pipeline              lighting_pipeline;
	Auto_exposure_compute_pipeline auto_exposure_pipeline;
	Bloom_pipeline                 bloom_pipeline;
	Composite_pipeline             composite_pipeline;

	void create(const Environment& env);
};
