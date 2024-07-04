#pragma once

#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

#include "vklib-algorithm.hpp"
#include "vklib-allocator.hpp"
#include "vklib-env.hpp"
#include "vklib-io.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-storage.hpp"
#include "vklib-sync.hpp"

#include <set>

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
	namespace data_parser
	{
		struct Parse_error : public Exception
		{
			Parse_error(const std::string& msg, const std::source_location& loc = std::source_location::current()) :
				Exception(std::format("Parser Error: {}", msg), loc)
			{
			}
		};

		template <typename T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <>
		std::vector<uint32_t> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <typename T>
		concept Glm_float_type = utility::glm_type_check::check_glm_type<T, float>();

		// Acquire data in accessor of type T, which is made of arbitrary number of `float` components.
		// `T` must be float or any of the glm genTypes.
		template <Glm_float_type T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		// Acquire data in accessor.
		// `T` must be float or any of the glm genTypes.
		// This function provides extra ability to decode normalized data to float according to glTF Spec.
		template <Glm_float_type T>
		std::vector<T> acquire_normalized_accessor(const tinygltf::Model& model, uint32_t accessor_idx);
	}

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
		Queue                 transfer_queue;
		Command_pool          command_pool;
		Vma_allocator         allocator;
		Device                device;
		Physical_device       physical_device;
		Descriptor_set_layout texture_descriptor_set_layout;
		Descriptor_set_layout albedo_only_layout;
		Fence                 fence;

		Loader_config config;

		std::vector<Buffer>         staging_buffers;
		std::vector<Command_buffer> command_buffers;

		Load_stage* load_stage   = nullptr;
		float*      sub_progress = nullptr;
	};

	struct Gltf_parse_error : public Exception
	{
		Gltf_parse_error(const std::string& msg, const std::source_location& loc = std::source_location::current()) :
			Exception(std::format("Gltf Parse Error: {}", msg), loc)
		{
		}
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

		Texture_view(
			const Loader_context&       context,
			const Texture&              texture,
			const vk::ComponentMapping& components = {}
		);

		vk::DescriptorImageInfo descriptor_info() const
		{
			return vk::DescriptorImageInfo()
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSampler(sampler)
				.setImageView(view);
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

		size_t    albedo_idx, metal_roughness_idx, normal_idx, emissive_idx, occlusion_idx;
		bool      double_sided;
		glm::vec3 emissive_strength;

		float      alpha_cutoff;
		Alpha_mode alpha_mode;

		struct Mat_params
		{
			alignas(16) glm::vec3 emissive_strength;
			alignas(4) float alpha_cutoff;
		} params;

		Descriptor_set descriptor_set, albedo_only_descriptor_set;
		Buffer         uniform_buffer;
	};

	using vertex = io::mesh::Mesh_vertex;

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

		uint32_t              mesh_idx;
		std::vector<uint32_t> children;

		Node_transformation transformation;

		void set(const tinygltf::Node& node);

		bool has_mesh() const { return mesh_idx != 0xFFFFFFFF; }
	};

	struct Primitive
	{
		uint32_t material_idx, vertex_count;

		uint32_t position_buffer, position_offset;
		uint32_t normal_buffer, normal_offset;
		uint32_t tangent_buffer, tangent_offset;
		uint32_t uv_buffer, uv_offset;

		glm::vec3 min, max;

		bool has_material() const { return material_idx != 0xFFFFFFFF; }
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
		Animation_parse_error(const std::string& msg, std::source_location loc = std::source_location::current()) :
			Gltf_parse_error(std::format("Animation Parse Error: {}", msg), loc)
		{
		}
	};

	struct Animation_error : Exception
	{
		Animation_error(const std::string& msg, std::source_location loc = std::source_location::current()) :
			Exception(std::format("Animation Error: {}", msg), loc)
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
			const std::set<Keyframe>::const_iterator& first,
			const std::set<Keyframe>::const_iterator& second,
			float                                     time
		) const;

		T cubic_interpolate(
			const std::set<Keyframe>::const_iterator& first,
			const std::set<Keyframe>::const_iterator& second,
			float                                     time
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
		uint32_t         node    = 0xffffffff;
		Animation_target target  = Animation_target::Translation;
		uint32_t         sampler = 0xffffffff;

		bool is_valid() const { return node != 0xffffffff && sampler != 0xffffffff; }

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

		void set_transformation(float time, const std::vector<Node>& node_list, std::unordered_map<uint32_t, Node_transformation>& map)
			const;
	};

	class Model
	{
	  public:

		std::vector<Texture>      textures;
		std::vector<Texture_view> texture_views;
		std::vector<Node>         nodes;
		std::vector<Material>     materials;
		std::vector<Mesh>         meshes;
		std::vector<Buffer>       vec3_buffers, vec2_buffers;
		std::vector<Scene>        scenes;
		std::vector<Animation>    animations;

		Descriptor_pool material_descriptor_pool;

		void load_gltf_ascii(Loader_context& loader_context, const std::string& path);

		void load_gltf_bin(Loader_context& loader_context, const std::string& path);

		void load_gltf_memory(Loader_context& loader_context, std::span<uint8_t> data);

	  private:

		struct Mesh_data_context
		{
			inline static constexpr size_t max_single_size = 64 * 1048576;  // MAX. 64M per block

			std::vector<std::vector<glm::vec3>> vec3_data;
			std::vector<std::vector<glm::vec2>> vec2_data;
		};

		void load(Loader_context& loader_context, const tinygltf::Model& gltf_model);

		void load_all_materials(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_scenes(const tinygltf::Model& gltf_model);
		void load_all_nodes(const tinygltf::Model& model);
		void load_all_meshes(Loader_context& loader_context, const tinygltf::Model& gltf_model);
		void load_all_animations(const tinygltf::Model& model);

		void generate_buffers(Loader_context& loader_context, const Mesh_data_context& mesh_context);

		void create_descriptor_pool(const Loader_context& context);

		Primitive parse_primitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Mesh_data_context& mesh_context);
	};
}

// Implementations
namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
	template <data_parser::Glm_float_type T>
	std::vector<T> data_parser::acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
	{
		std::vector<T> dst;

		const auto& accessor    = model.accessors[accessor_idx];
		const auto& buffer_view = model.bufferViews[accessor.bufferView];
		const auto& buffer      = model.buffers[buffer_view.buffer];

		const auto* ptr = reinterpret_cast<const T*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
		dst.resize(accessor.count);

		if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
			throw Parse_error(std::format("Data type incompatible with float32: {}", accessor.componentType));

		std::copy(ptr, ptr + accessor.count, dst.begin());

		return dst;
	}

	template <data_parser::Glm_float_type T>
	std::vector<T> data_parser::acquire_normalized_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
	{
		std::vector<T> dst;

		const auto& accessor    = model.accessors[accessor_idx];
		const auto& buffer_view = model.bufferViews[accessor.bufferView];
		const auto& buffer      = model.buffers[buffer_view.buffer];

		const auto* ptr = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
		dst.resize(accessor.count);

		static constexpr auto supported_formats = std::to_array(
			{TINYGLTF_COMPONENT_TYPE_FLOAT,
			 TINYGLTF_COMPONENT_TYPE_BYTE,
			 TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
			 TINYGLTF_COMPONENT_TYPE_SHORT,
			 TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT}
		);

		if (std::count(supported_formats.begin(), supported_formats.end(), accessor.componentType) == 0)
			throw Parse_error(std::format("Data type incompatible with float32: {}", accessor.componentType));

		auto copy_to_dst = [&](const auto* ptr, auto transformation)
		{
			for (auto i : Range(sizeof(T) / sizeof(float) * accessor.count))
				reinterpret_cast<float*>(dst.data())[i] = transformation(ptr[i]);
		};

		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
		{
			const auto* fptr = reinterpret_cast<const T*>(ptr);
			std::copy(fptr, fptr + accessor.count, dst.begin());
			break;
		}

		case TINYGLTF_COMPONENT_TYPE_BYTE:
		{
			auto transformation = [](int8_t val) -> float
			{
				return std::max(val / 127.0, -1.0);
			};
			copy_to_dst((const int8_t*)ptr, transformation);
		}

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			auto transformation = [](uint8_t val) -> float
			{
				return val / 255.0;
			};
			copy_to_dst((const uint8_t*)ptr, transformation);
		}

		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			auto transformation = [](int16_t val) -> float
			{
				return std::max(val / 32767.0, -1.0);
			};
			copy_to_dst((const int16_t*)ptr, transformation);
		}

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			auto transformation = [](uint16_t val) -> float
			{
				return val / 65535.0;
			};
			copy_to_dst((const uint16_t*)ptr, transformation);
		}
		}

		return dst;
	}

	template <Vec3_or_quat T>
	T Animation_sampler<T>::operator[](float time) const
	{
		const auto upper = std::upper_bound(
			keyframes.begin(),
			keyframes.end(),
			time,
			[](float time, auto val)
			{
				return time < val.time;
			}
		);

		// time larger than upper bound
		if (upper == keyframes.end()) return keyframes.rbegin()->linear;

		// time smaller than lower bound
		if (upper == keyframes.begin()) return keyframes.begin()->linear;

		auto lower = upper;
		lower--;

		switch (mode)
		{
		case Interpolation_mode::Step:
			return lower->linear;

		case Interpolation_mode::Linear:
			return linear_interpolate(lower, upper, time);

		case Interpolation_mode::Cubic_spline:
			return cubic_interpolate(lower, upper, time);

		default:
			throw Animation_error("Unknown mode");
		}
	}
}