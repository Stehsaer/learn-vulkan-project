#pragma once

#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

#include "vklib/core/algorithm.hpp"
#include "vklib/core/allocator.hpp"
#include "vklib/core/env.hpp"
#include "vklib/core/io.hpp"
#include "vklib/core/pipeline.hpp"
#include "vklib/core/storage.hpp"
#include "vklib/core/sync.hpp"


#include <set>
#include <unordered_set>

namespace VKLIB_HPP_NAMESPACE::io::gltf
{
	inline std::optional<uint32_t> to_optional(int val)
	{
		if (val < 0)
			return std::nullopt;
		else
			return (uint32_t)val;
	}

	struct Gltf_parse_error : public Exception
	{
		std::string description;

		Gltf_parse_error(
			const std::string&          msg,
			const std::string&          description,
			const std::source_location& loc = std::source_location::current()
		) :
			Exception(std::format("(Gltf Parse Error) {}", msg), description, loc)
		{
		}
	};

	struct Gltf_spec_violation : public Gltf_parse_error
	{
		std::string reference;

		Gltf_spec_violation(
			const std::string&          msg,
			const std::string&          description,
			const std::string&          reference,
			const std::source_location& loc = std::source_location::current()
		) :
			Gltf_parse_error(std::format("Violation of Gltf Spec: {}", msg), std::format("{}. See: {}", description, reference), loc),
			reference(reference)
		{
		}
	};

	struct Loader_config
	{
		bool  enable_anistropy = false;
		float max_anistropy    = 1.0;
	};

	enum class Load_stage
	{
		Uninitialized    = 0,
		Tinygltf_loading = 1,
		Load_material    = 2,
		Load_mesh        = 3,
		Finished         = 4,
		Error            = -1,
	};

	struct Loader_context
	{
		Queue           transfer_queue;
		Command_pool    command_pool;
		Vma_allocator   allocator;
		Device          device;
		Physical_device physical_device;
		Fence           fence;

		Loader_config config;

		std::vector<Buffer>         staging_buffers;
		std::vector<Command_buffer> command_buffers;

		Load_stage* load_stage   = nullptr;
		float*      sub_progress = nullptr;
	};

	struct Texture
	{
		std::string name;

		uint32_t   width, height;
		uint32_t   mipmap_levels;
		uint32_t   component_count;
		vk::Format format;

		Image image;

		void parse(const tinygltf::Image& tex, Loader_context& loader_context);

		void generate(uint8_t value0, uint8_t value1, uint8_t value2, uint8_t value3, Loader_context& loader_context);
	};

	struct Texture_view
	{
		Image_view    view;
		Image_sampler sampler;

		Texture_view() = default;

		Texture_view(
			const Loader_context&       context,
			const Texture&              texture,
			const tinygltf::Sampler&    sampler,
			const vk::ComponentMapping& components = {}
		);

		Texture_view(const Loader_context& context, const Texture& texture, const vk::ComponentMapping& components = {});

		Texture_view(
			const Loader_context&       context,
			const std::vector<Texture>& texture_list,
			const tinygltf::Model&      model,
			const tinygltf::Texture&    texture,
			const vk::ComponentMapping& components = {}
		);

		vk::DescriptorImageInfo descriptor_info(vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) const
		{
			return vk::DescriptorImageInfo().setImageLayout(layout).setSampler(sampler).setImageView(view);
		}
	};

	enum class Alpha_mode
	{
		Opaque = 0,
		Mask,
		Blend
	};

	struct Material
	{
		std::string name;

		uint32_t albedo_idx, metal_roughness_idx, normal_idx, emissive_idx, occlusion_idx;
		bool     double_sided;

		Alpha_mode alpha_mode = Alpha_mode::Opaque;

		struct Mat_params
		{
			alignas(16) glm::vec3 emissive_multiplier{1.0};
			alignas(16) glm::vec2 metalness_roughness_multiplier{1.0};
			alignas(16) glm::vec3 base_color_multiplier{1.0};
			alignas(4) float alpha_cutoff{0.5};
			alignas(4) float normal_scale{1.0};
			alignas(4) float occlusion_strength{1.0};
			alignas(4) float emissive_strength{1.0};
		} params;
	};

	struct Node_transformation
	{
		glm::quat rotation{1.0, 0.0, 0.0, 0.0};
		glm::vec3 translation{0.0};
		glm::vec3 scale{1.0};

		glm::mat4 get_mat4() const;
		void      set(const tinygltf::Node& node);
	};

	struct Node
	{
		std::string name;

		std::optional<uint32_t> mesh_idx;
		std::vector<uint32_t>   children;

		Node_transformation transformation;

		std::optional<uint32_t> skin_idx;

		void set(const tinygltf::Node& node);
	};

	struct Skin
	{
		std::vector<uint32_t>  joints;
		std::vector<glm::mat4> inverse_bind_matrices;
	};

	struct Primitive_skin
	{
		uint32_t joint_buffer, joint_offset;
		uint32_t weight_buffer, weight_offset;
	};

	struct Primitive
	{
		bool enabled = true;

		std::optional<uint32_t> material_idx;

		uint32_t vertex_count;

		uint32_t position_buffer, position_offset;
		uint32_t normal_buffer, normal_offset;
		uint32_t tangent_buffer, tangent_offset;
		uint32_t uv_buffer, uv_offset;

		std::optional<Primitive_skin> skin = std::nullopt;

		glm::vec3 min, max;
	};

	struct Mesh
	{
		std::string name;

		std::vector<Primitive> primitives;
	};

	struct Scene
	{
		std::string name;

		std::vector<uint32_t> nodes;
	};

	/* Animation */

	template <typename T>
	concept Vec3_or_quat = std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::quat>;

	struct Animation_parse_error : Gltf_parse_error
	{
		Animation_parse_error(
			const std::string&   msg,
			const std::string&   detail = "",
			std::source_location loc    = std::source_location::current()
		) :
			Gltf_parse_error(std::format("Error while parsing animation {}", msg), detail, loc)
		{
		}
	};

	struct Animation_runtime_error : Exception
	{
		Animation_runtime_error(
			const std::string&   msg,
			const std::string&   detail = "",
			std::source_location loc    = std::source_location::current()
		) :
			Exception(std::format("Animation Error: {}", msg), detail, loc)
		{
		}
	};

	enum class Interpolation_mode
	{
		Linear,
		Cubic_spline,
		Step
	};

	template <Vec3_or_quat T>
	class Animation_sampler
	{
	  public:

		// 3 parameters for Cubic
		struct Cubic_params
		{
			T v, a, b;
		};

		struct Keyframe
		{
			float time = 0.0;

			union
			{
				// Linear
				T linear;

				// Cubic
				Cubic_params cubic;
			};

			bool operator<(const Keyframe& other) const { return time < other.time; }

			Keyframe() = default;
			Keyframe(float time, const T& linear) :
				time(time),
				linear(linear)
			{
			}

			Keyframe(float time, const T& v, const T& a, const T& b) :
				time(time),
				cubic(v, a, b)
			{
			}
		};

		std::set<Keyframe> keyframes;
		Interpolation_mode mode = Interpolation_mode::Linear;

		void load(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler);

		float start_time() const { return keyframes.begin()->time; }
		float end_time() const { return keyframes.rbegin()->time; }

		// Get the interpolated value at given time
		T operator[](float time) const;

	  private:

		T linear_interpolate(
			const typename std::set<Keyframe>::const_iterator& first,
			const typename std::set<Keyframe>::const_iterator& second,
			float                                              time
		) const;

		T cubic_interpolate(
			const typename std::set<Keyframe>::const_iterator& first,
			const typename std::set<Keyframe>::const_iterator& second,
			float                                              time
		) const;
	};

	std::variant<Animation_sampler<glm::vec3>, Animation_sampler<glm::quat>> load_sampler(
		const tinygltf::Model&            model,
		const tinygltf::AnimationSampler& sampler
	);

	enum class Animation_target
	{
		Translation,
		Rotation,
		Scale,
		Weights
	};

	struct Animation_channel
	{
		std::optional<uint32_t> node;
		Animation_target        target = Animation_target::Translation;
		std::optional<uint32_t> sampler;

		bool is_valid() const { return node && sampler; }

		// Parse animation channel, if not valid returns `false`
		bool load(const tinygltf::AnimationChannel& animation);
	};

	struct Animation
	{
		std::string name;
		float       start_time, end_time;

		std::vector<Animation_channel>                                                        channels;
		std::vector<std::variant<Animation_sampler<glm::vec3>, Animation_sampler<glm::quat>>> samplers;

		void load(const tinygltf::Model& model, const tinygltf::Animation& animation);

		void set_transformation(float time, const std::function<Node_transformation&(uint32_t)>& func) const;
	};

	struct Camera
	{
		float znear = 0.01, zfar;

		std::string name;

		struct Ortho
		{
			float xmag, ymag;
		};

		struct Perspective
		{
			float yfov, aspect_ratio;
		};

		union
		{
			Ortho       ortho;
			Perspective perspective;
		};

		bool is_ortho;

		std::optional<uint32_t> node_idx;

		Camera(const tinygltf::Camera& camera);
	};

	class Model
	{
	  public:

		std::vector<Texture>      textures;
		std::vector<Texture_view> texture_views;
		std::vector<Node>         nodes;
		std::vector<Material>     materials;
		std::vector<Mesh>         meshes;
		std::vector<Buffer>       vec3_buffers, vec2_buffers, joint_buffers, weight_buffers;
		std::vector<Scene>        scenes;
		std::vector<Animation>    animations;
		std::vector<Skin>         skins;
		std::vector<Camera>       cameras;

		Descriptor_pool material_descriptor_pool;

		void load_gltf_ascii(Loader_context& loader_context, const std::string& path);

		void load_gltf_bin(Loader_context& loader_context, const std::string& path);

		void load_gltf_memory(Loader_context& loader_context, std::span<uint8_t> data);

	  private:

		struct Mesh_data_context
		{
			inline static constexpr size_t max_single_size = 64 * 1048576;  // MAX. 64M per block

			std::vector<std::vector<glm::vec3>>    vec3_data;
			std::vector<std::vector<glm::vec2>>    vec2_data;
			std::vector<std::vector<glm::u16vec4>> joint_data;
			std::vector<std::vector<glm::vec4>>    weight_data;
		};

		void load(Loader_context& loader_context, const tinygltf::Model& gltf_model);

		void load_all_materials(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_images(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_textures(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_scenes(const tinygltf::Model& gltf_model);
		void load_all_nodes(const tinygltf::Model& model);
		void load_all_meshes(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_animations(const tinygltf::Model& model);
		void load_all_skins(const tinygltf::Model& model);
		void load_all_cameras(const tinygltf::Model& model);

		void generate_buffers(Loader_context& loader_context, const Mesh_data_context& mesh_context);

		Primitive parse_primitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Mesh_data_context& mesh_context);
	};
}
