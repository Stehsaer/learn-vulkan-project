#pragma once
#include "environment.hpp"
#include "pipeline.hpp"

struct Write_descriptor_base
{
	inline static constexpr size_t write_dynamic_count                     = std::numeric_limits<size_t>::max();
	virtual                        operator vk::WriteDescriptorSet() const = 0;
};

template <size_t Count, vk::DescriptorType Type, typename Vk_info_type>
	requires(
		Count != 0 && (std::is_same_v<Vk_info_type, vk::DescriptorBufferInfo> || std::is_same_v<Vk_info_type, vk::DescriptorImageInfo>)
	)
struct Write_descriptor_template : public Write_descriptor_base
{
	using Info_type = std::conditional_t<
		Count == 1,
		Vk_info_type,
		std::conditional_t<
			Count == Write_descriptor_base::write_dynamic_count,
			std::vector<Vk_info_type>,
			std::array<Vk_info_type, Count>>>;

	vk::DescriptorSet set;
	uint32_t          binding;
	Info_type         info;

	Write_descriptor_template() = default;

	Write_descriptor_template(const Descriptor_set& set, uint32_t binding)
	{
		this->set     = set;
		this->binding = binding;
	}

	Write_descriptor_template& set_info(const Vk_info_type& info)
		requires(Count == 1)
	{
		this->info = info;
		return *this;
	}

	Vk_info_type& operator[](size_t idx)
		requires(Count > 1 && Count != write_dynamic_count)
	{
		return info[idx];
	}

	Write_descriptor_template& add(const Vk_info_type& info)
		requires(Count == write_dynamic_count)
	{
		this->info.push_back(info);
		return *this;
	}

	virtual operator vk::WriteDescriptorSet() const
	{
		auto ret = vk::WriteDescriptorSet().setDstBinding(binding).setDstSet(set).setDescriptorCount(Count).setDescriptorType(Type);

		if constexpr (std::is_same_v<Vk_info_type, vk::DescriptorBufferInfo>)
		{
			if constexpr (Count == 1)
				ret.pBufferInfo = &info;
			else
				ret.pBufferInfo = info.data();
		}
		else
		{
			if constexpr (Count == 1)
				ret.pImageInfo = &info;
			else
				ret.pImageInfo = info.data();
		}

		return ret;
	}
};

template <size_t Count = 1, vk::DescriptorType Type = vk::DescriptorType::eUniformBuffer>
using Write_descriptor_buffer = Write_descriptor_template<Count, Type, vk::DescriptorBufferInfo>;

template <size_t Count = 1, vk::DescriptorType Type = vk::DescriptorType::eCombinedImageSampler>
using Write_descriptor_image = Write_descriptor_template<Count, Type, vk::DescriptorImageInfo>;

struct Shadow_rt
{
	std::array<Image, csm_count>       shadow_images;       // cascaded shadow maps
	std::array<Image_view, csm_count>  shadow_image_views;  // corresponding image views
	std::array<Framebuffer, csm_count> shadow_framebuffers;

	std::array<Buffer, csm_count>         shadow_matrix_uniform;  // @vert, set = 0, binding = 0
	std::array<Descriptor_set, csm_count> shadow_matrix_descriptor_set;

	void create(
		const Environment&                     env,
		const Render_pass&                     render_pass,
		const Descriptor_pool&                 pool,
		const Descriptor_set_layout&           layout,
		const std::array<uint32_t, csm_count>& shadow_map_size
	);

	std::array<Write_descriptor_buffer<>, csm_count> update_uniform(const std::array<Shadow_pipeline::Shadow_uniform, csm_count>& data
	);
};

struct Gbuffer_rt
{
	Image       normal, albedo, pbr, emissive, depth;
	Image_view  normal_view, albedo_view, pbr_view, emissive_view, depth_view;
	Framebuffer framebuffer;

	Buffer         camera_uniform_buffer;  // @vert, set = 0, binding = 0
	Descriptor_set camera_uniform_descriptor_set;

	void create(const Environment& env, const Render_pass& render_pass, const Descriptor_pool& pool, const Descriptor_set_layout& layout);

	Write_descriptor_buffer<> update_uniform(const Gbuffer_pipeline::Camera_uniform& data);
};

struct Lighting_rt
{
	Image       luminance, brightness;
	Image_view  luminance_view, brightness_view;
	Framebuffer framebuffer;

	Image_sampler input_sampler, shadow_map_sampler;

	Buffer         transmat_buffer;  // @frag, set = 0, binding = 6
	Descriptor_set input_descriptor_set;

	void create(const Environment& env, const Render_pass& render_pass, const Descriptor_pool& pool, const Descriptor_set_layout& layout);

	std::array<Write_descriptor_image<>, 5> link_gbuffer(const Gbuffer_rt& gbuffer);
	Write_descriptor_image<csm_count>       link_shadow(const Shadow_rt& shadow);

	Write_descriptor_buffer<> update_uniform(const Lighting_pipeline::Params& data);
};

struct Auto_exposure_compute_rt
{
	Buffer out_buffer, medium_buffer;

	std::vector<Descriptor_set> luminance_avg_descriptor_sets;
	Descriptor_set              lerp_descriptor_set;

	void create(const Environment& env, const Descriptor_pool& pool, const Auto_exposure_compute_pipeline& pipeline, uint32_t count);

	std::tuple<
		std::vector<Write_descriptor_image<1, vk::DescriptorType::eStorageImage>>,
		std::vector<Write_descriptor_buffer<1, vk::DescriptorType::eStorageBuffer>>>
	link_lighting(const std::vector<const Lighting_rt*>& rt);

	std::array<Write_descriptor_buffer<1, vk::DescriptorType::eStorageBuffer>, 2> link_self();
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

	void create(const Environment& env, const Descriptor_pool& pool, const Bloom_pipeline& pipeline);

	std::tuple<std::array<Write_descriptor_image<1, vk::DescriptorType::eStorageImage>, 2>, Write_descriptor_buffer<>>
	link_bloom_filter(const Lighting_rt& lighting, const Auto_exposure_compute_rt& exposure);

	std::array<
		std::tuple<
			Write_descriptor_image<1, vk::DescriptorType::eStorageImage>,
			Write_descriptor_image<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
	link_bloom_blur();

	std::array<
		std::tuple<
			Write_descriptor_image<>,
			Write_descriptor_image<1, vk::DescriptorType::eStorageImage>,
			Write_descriptor_image<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
	link_bloom_acc();
};

struct Composite_rt
{
	Image       composite_output;
	Image_view  image_view;
	Framebuffer framebuffer;

	Image_sampler input_sampler;

	Buffer         params_buffer;  // @frag, set = 0, binding = 1
	Descriptor_set descriptor_set;

	void create(
		const Environment&           env,
		const Render_pass&           render_pass,
		const Descriptor_pool&       pool,
		const Descriptor_set_layout& layout
	);

	Write_descriptor_image<>  link_lighting(const Lighting_rt& lighting);
	Write_descriptor_buffer<> link_auto_exposure(const Auto_exposure_compute_rt& rt);
	Write_descriptor_image<>  link_bloom(const Bloom_rt& bloom);
	Write_descriptor_buffer<> update_uniform(const Composite_pipeline::Exposure_param& data);
};

struct Fxaa_rt
{
	Image_view  image_view;
	Framebuffer framebuffer;

	Image_sampler input_sampler;

	Buffer         params_buffer;
	Descriptor_set descriptor_set;

	void create(
		const Environment&           env,
		const Render_pass&           render_pass,
		const Descriptor_pool&       pool,
		const Descriptor_set_layout& layout,
		uint32_t                     idx
	);

	Write_descriptor_image<>  link_composite(const Composite_rt& composite);
	Write_descriptor_buffer<> update_uniform(const Fxaa_pipeline::Params& param);
};

inline static std::array<uint32_t, 3> shadow_map_res{
	{2048, 2048, 1536}
};

// Accumulate descriptor pool sizes
class Pool_size_calculator
{
  public:

	Pool_size_calculator& add(std::span<const vk::DescriptorPoolSize> pool_sizes, size_t count = 1)
	{
		for (const auto& item : pool_sizes)
		{
			auto find = size_map.find(item.type);

			if (find == size_map.end())
				size_map[item.type] = item.descriptorCount * count;
			else
				find->second += item.descriptorCount * count;
		}

		return *this;
	}

	std::vector<vk::DescriptorPoolSize> get() const
	{
		std::vector<vk::DescriptorPoolSize> ret;
		ret.reserve(size_map.size());
		for (const auto& item : size_map) ret.emplace_back(item.first, item.second);

		return ret;
	}

  private:

	std::map<vk::DescriptorType, size_t> size_map;
};

struct Render_target_set
{
	Descriptor_pool shared_pool;

	Shadow_rt    shadow_rt;
	Gbuffer_rt   gbuffer_rt;
	Lighting_rt  lighting_rt;
	Bloom_rt     bloom_rt;
	Composite_rt composite_rt;
	Fxaa_rt      fxaa_rt;

	void create(const Environment& env, const Pipeline_set& pipeline, uint32_t idx);
	void link(const Environment& env, const Auto_exposure_compute_rt& auto_exposure_rt);

	void update(
		const Environment&                                            env,
		const std::array<Shadow_pipeline::Shadow_uniform, csm_count>& shadow_matrices,
		const Gbuffer_pipeline::Camera_uniform&                       gbuffer_camera,
		const Lighting_pipeline::Params&                              lighting_param,
		const Composite_pipeline::Exposure_param&                     composite_exposure_param
	);
};

struct Render_targets
{
	Descriptor_pool          shared_pool;
	Auto_exposure_compute_rt auto_exposure_rt;

	std::vector<Render_target_set> render_target_set;

	Semaphore acquire_semaphore, render_done_semaphore;
	Fence     next_frame_fence;

	Render_target_set&       operator[](size_t idx) { return render_target_set[idx]; }
	const Render_target_set& operator[](size_t idx) const { return render_target_set[idx]; }

	void create(const Environment& env, const Pipeline_set& pipeline);

	void create_sync_objects(const Environment& env);

	void link(const Environment& env);
};
