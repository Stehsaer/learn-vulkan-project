#pragma once
#include "app-basic.hpp"

inline constexpr uint32_t csm_count              = 3;
inline constexpr uint32_t bloom_downsample_count = 8;

#pragma region "Pipeline"

struct Shadow_pipeline
{
	static constexpr vk::Format shadow_map_format = vk::Format::eD32Sfloat;

	// At Shadow Vert, set = 0
	struct Shadow_uniform
	{
		glm::mat4 shadow_matrix;
	};

	// At Shadow Vert, push_constant
	struct Model_matrix
	{
		glm::mat4 matrix;
	};

	Descriptor_set_layout descriptor_set_layout_shadow_matrix,  // @vert, set = 0
		descriptor_set_layout_texture;                          // @frag, set = 1

	Pipeline_layout   pipeline_layout;
	Graphics_pipeline pipeline, double_sided_pipeline;
	Render_pass       render_pass;

	static vk::ClearValue clear_value;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer, csm_count}
	});

	void create(const App_environment& env);
};

struct Gbuffer_pipeline
{
	static constexpr vk::Format normal_format = vk::Format::eR32G32B32A32Sfloat,
								color_format = vk::Format::eR8G8B8A8Unorm, emissive_format = vk::Format::eR8G8B8A8Unorm,
								depth_format = vk::Format::eD32Sfloat;

	// At Gbuffer Vert, set = 0
	struct Camera_uniform
	{
		glm::mat4 view_projection_matrix;
	};

	// At Gbuffer Vert, push_constant
	struct Model_matrix
	{
		glm::mat4 matrix;
	};

	Descriptor_set_layout descriptor_set_layout_texture,  // @frag, set = 1
		descriptor_set_layout_camera;                     // @vert, set = 0, binding = 0

	Pipeline_layout   pipeline_layout;
	Graphics_pipeline pipeline, double_sided_pipeline;
	Render_pass       render_pass;

	static std::array<vk::ClearValue, 5> clear_values;

	inline static auto descriptor_pool_size = std::to_array<vk::DescriptorPoolSize>({
		{vk::DescriptorType::eUniformBuffer,        2},
		{vk::DescriptorType::eCombinedImageSampler, 5}
	});

	void create(const App_environment& env, const App_swapchain& swapchain);
};

struct Lighting_pipeline
{
	static constexpr vk::Format luminance_format = vk::Format::eR16G16B16A16Sfloat;

	// At Lighting Frag, push_constant
	struct Trans_mat
	{
		alignas(16) glm::mat4 view_projection_inv;
		alignas(16) glm::vec3 camera_position;
		alignas(16) glm::mat4 shadow[3];
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

	void create(const App_environment& env, const App_swapchain& swapchain);
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

	void create(const App_environment& env);
};

struct Bloom_pipeline
{
	using Exposure_result = Auto_exposure_compute_pipeline::Exposure_result;

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

	void create(const App_environment& env);
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

	void create(const App_environment& env, const App_swapchain& swapchain);
};

#pragma endregion

#pragma region "Render Target"

struct Descriptor_update_base
{
	virtual vk::WriteDescriptorSet write_info() const = 0;
};

template <size_t Count = 1, vk::DescriptorType Type = vk::DescriptorType::eUniformBuffer>
	requires(Count != 0)
struct Descriptor_buffer_update : public Descriptor_update_base
{
	using Info_type
		= std::conditional_t<Count == 1, vk::DescriptorBufferInfo, std::array<vk::DescriptorBufferInfo, Count>>;

	vk::DescriptorSet set;
	uint32_t          binding;
	Info_type         info;

	Descriptor_buffer_update(const Descriptor_set& set, uint32_t binding, const Info_type& info) :
		set(set),
		binding(binding),
		info(info)
	{
	}

	Descriptor_buffer_update() = default;

	virtual vk::WriteDescriptorSet write_info() const
	{
		auto ret = vk::WriteDescriptorSet()
					   .setDstBinding(binding)
					   .setDstSet(set)
					   .setDescriptorCount(Count)
					   .setDescriptorType(Type);

		if constexpr (Count == 1)
			ret.pBufferInfo = &info;
		else
			ret.pBufferInfo = info.data();

		return ret;
	}
};

template <size_t Count = 1, vk::DescriptorType Type = vk::DescriptorType::eCombinedImageSampler>
	requires(Count != 0)
struct Descriptor_image_update : public Descriptor_update_base
{
	using Info_type
		= std::conditional_t<Count == 1, vk::DescriptorImageInfo, std::array<vk::DescriptorImageInfo, Count>>;

	vk::DescriptorSet set;
	uint32_t          binding;
	Info_type         info;

	Descriptor_image_update(const Descriptor_set& set, uint32_t binding, const Info_type& info) :
		set(set),
		binding(binding),
		info(info)
	{
	}

	Descriptor_image_update() = default;

	virtual vk::WriteDescriptorSet write_info() const
	{
		auto ret = vk::WriteDescriptorSet()
					   .setDstBinding(binding)
					   .setDstSet(set)
					   .setDescriptorCount(Count)
					   .setDescriptorType(Type);

		if constexpr (Count == 1)
			ret.pImageInfo = &info;
		else
			ret.pImageInfo = info.data();

		return ret;
	}
};

struct Shadow_rt
{
	std::array<Image, csm_count>       shadow_images;       // cascaded shadow maps
	std::array<Image_view, csm_count>  shadow_image_views;  // corresponding image views
	std::array<Framebuffer, csm_count> shadow_framebuffers;

	std::array<Buffer, csm_count>         shadow_matrix_uniform;  // @vert, set = 0, binding = 0
	std::array<Descriptor_set, csm_count> shadow_matrix_descriptor_set;

	void create(
		const App_environment&                 env,
		const Render_pass&                     render_pass,
		const Descriptor_pool&                 pool,
		const Descriptor_set_layout&           layout,
		const std::array<uint32_t, csm_count>& shadow_map_size
	);

	std::array<Descriptor_buffer_update<>, csm_count> update_uniform(
		const std::array<Shadow_pipeline::Shadow_uniform, csm_count>& data
	);
};

struct Gbuffer_rt
{
	Image       normal, albedo, pbr, emissive, depth;
	Image_view  normal_view, albedo_view, pbr_view, emissive_view, depth_view;
	Framebuffer framebuffer;

	Buffer         camera_uniform_buffer;  // @vert, set = 0, binding = 0
	Descriptor_set camera_uniform_descriptor_set;

	void create(
		const App_environment&       env,
		const Render_pass&           render_pass,
		const Descriptor_pool&       pool,
		const Descriptor_set_layout& layout,
		const App_swapchain&         swapchain
	);

	Descriptor_buffer_update<> update_uniform(const Gbuffer_pipeline::Camera_uniform& data);
};

struct Lighting_rt
{
	Image       luminance, brightness;
	Image_view  luminance_view, brightness_view;
	Framebuffer framebuffer;

	Image_sampler input_sampler, shadow_map_sampler;

	Buffer         transmat_buffer;  // @frag, set = 0, binding = 6
	Descriptor_set input_descriptor_set;

	void create(
		const App_environment&       env,
		const Render_pass&           render_pass,
		const Descriptor_pool&       pool,
		const Descriptor_set_layout& layout,
		const App_swapchain&         swapchain
	);

	std::array<Descriptor_image_update<>, 5> link_gbuffer(const Gbuffer_rt& gbuffer);
	Descriptor_image_update<csm_count>       link_shadow(const Shadow_rt& shadow);

	Descriptor_buffer_update<> update_uniform(const Lighting_pipeline::Trans_mat& data);
};

struct Auto_exposure_compute_rt
{
	Buffer out_buffer, medium_buffer;

	std::vector<Descriptor_set> luminance_avg_descriptor_sets;
	Descriptor_set              lerp_descriptor_set;

	void create(
		const App_environment&                env,
		const Descriptor_pool&                pool,
		const Auto_exposure_compute_pipeline& pipeline,
		uint32_t                              count
	);

	std::tuple<
		std::vector<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
		std::vector<Descriptor_buffer_update<1, vk::DescriptorType::eStorageBuffer>>>
	link_lighting(const std::vector<Lighting_rt>& rt);

	std::array<Descriptor_buffer_update<1, vk::DescriptorType::eStorageBuffer>, 2> link_self();
};

struct Bloom_rt
{
	Image bloom_downsample_chain, bloom_upsample_chain;

	std::array<Image_view, bloom_downsample_count>     downsample_chain_view;
	std::array<Image_view, bloom_downsample_count - 2> upsample_chain_view;

	Image_sampler upsample_chain_sampler;

	std::array<vk::Extent2D, bloom_downsample_count> extents;

	Descriptor_set bloom_filter_descriptor_set;

	std::vector<Descriptor_set> bloom_blur_descriptor_sets, bloom_acc_descriptor_sets;

	void create(
		const App_environment& env,
		const App_swapchain&   swapchain,
		const Descriptor_pool& pool,
		const Bloom_pipeline&  pipeline
	);

	std::tuple<std::array<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>, 2>, Descriptor_buffer_update<>>
	link_bloom_filter(const Lighting_rt& lighting, const Auto_exposure_compute_rt& exposure);

	std::array<
		std::tuple<
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>,
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
	link_bloom_blur();

	std::array<
		std::tuple<
			Descriptor_image_update<>,
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>,
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
	link_bloom_acc();
};

struct Composite_rt
{
	Image_view  image_view;
	Framebuffer framebuffer;

	Image_sampler input_sampler;

	Buffer         params_buffer;  // @frag, set = 0, binding = 1
	Descriptor_set descriptor_set;

	void create(
		const App_environment&       env,
		const Render_pass&           render_pass,
		const Descriptor_pool&       pool,
		const Descriptor_set_layout& layout,
		const App_swapchain&         swapchain,
		uint32_t                     idx
	);

	Descriptor_image_update<>  link_lighting(const Lighting_rt& lighting);
	Descriptor_buffer_update<> link_auto_exposure(const Auto_exposure_compute_rt& rt);
	Descriptor_image_update<>  link_bloom(const Bloom_rt& bloom);
	Descriptor_buffer_update<> update_uniform(const Composite_pipeline::Exposure_param& data);
};

#pragma endregion

struct Main_pipeline
{
	Shadow_pipeline                shadow_pipeline;
	Gbuffer_pipeline               gbuffer_pipeline;
	Lighting_pipeline              lighting_pipeline;
	Auto_exposure_compute_pipeline auto_exposure_pipeline;
	Bloom_pipeline                 bloom_pipeline;
	Composite_pipeline             composite_pipeline;

	void create(const App_environment& env, const App_swapchain& swapchain);
};

struct Main_rt
{
	std::array<uint32_t, 3> shadow_map_res{
		{2048, 1536, 1024}
	};

	Descriptor_pool shared_pool;

	std::vector<Shadow_rt>    shadow_rt;
	std::vector<Gbuffer_rt>   gbuffer_rt;
	std::vector<Lighting_rt>  lighting_rt;
	Auto_exposure_compute_rt  auto_exposure_rt;
	std::vector<Bloom_rt>     bloom_rt;
	std::vector<Composite_rt> composite_rt;

	Semaphore acquire_semaphore, render_done_semaphore;
	Fence     next_frame_fence;

	void create_sync_objects(const App_environment& env);

	void create(
		const App_environment& env,
		const App_swapchain&   swapchain,
		const Main_pipeline&   pipeline,
		bool                   recreate = false
	);

	void update_uniform(
		const App_environment&                  env,
		uint32_t                                idx,
		const glm::mat4&                        view_projection,
		const glm::vec3&                        camera_position,
		const std::array<glm::mat4, csm_count>& shadow,
		const glm::vec3&                        sunlight_pos,
		const glm::vec3&                        sunlight_color,
		float                                   exposure,
		float                                   emissive_brightness,
		float                                   skybox_brightness,
		float                                   bloom_intensity
	);
};

class Model_renderer
{
  private:

	std::vector<std::tuple<uint32_t, uint32_t>> single_sided, double_sided;

  public:

	struct Draw_result
	{
		float  near, far;
		double time_consumed;
	};

	Draw_result render_gltf(
		const Command_buffer&                                       command_buffer,
		const io::mesh::gltf::Model&                                model,
		const algorithm::frustum_culling::Frustum&                  frustum,
		const glm::vec3&                                            eye_position,
		const glm::vec3&                                            eye_path,
		const Graphics_pipeline&                                    single_pipeline,
		const Graphics_pipeline&                                    double_pipeline,
		const Pipeline_layout&                                      pipeline_layout,
		const std::function<void(const io::mesh::gltf::Material&)>& bind_func
	);

	void render_node(
		const io::mesh::gltf::Model&               model,
		uint32_t                                   idx,
		const algorithm::frustum_culling::Frustum& frustum,
		const glm::vec3&                           eye_position,
		const glm::vec3&                           eye_path,
		float&                                     near,
		float&                                     far
	);

	uint32_t get_object_count() const { return single_sided.size() + double_sided.size(); }
};
