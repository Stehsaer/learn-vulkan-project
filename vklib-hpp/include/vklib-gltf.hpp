#pragma once

#include "vklib-io.hpp"
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"
#include "vklib-algorithm.hpp"
#include "vklib-allocator.hpp"
#include "vklib-env.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-storage.hpp"
#include "vklib-sync.hpp"

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
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

	struct Node
	{
		std::string name;

		uint32_t              mesh_idx;
		std::vector<uint32_t> children;

		glm::quat rotation{1.0, 0.0, 0.0, 0.0};
		glm::vec3 translation{0.0};
		glm::vec3 scale{1.0};

		glm::mat4 get_transformation() const;
		void      set_transformation(const tinygltf::Node& node);
	};

	struct Primitive
	{
		uint32_t material_idx, vertex_count;

		uint32_t position_buffer, position_offset;
		uint32_t normal_buffer, normal_offset;
		uint32_t tangent_buffer, tangent_offset;
		uint32_t uv_buffer, uv_offset;

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

		void generate_buffers(Loader_context& loader_context, const Mesh_data_context& mesh_context);

		void create_descriptor_pool(const Loader_context& context);

		Primitive parse_primitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Mesh_data_context& mesh_context);
	};
}
