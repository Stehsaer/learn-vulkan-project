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
	struct Vertex_accessor_set
	{
		int position, normal, texcoord;

		bool operator==(const Vertex_accessor_set& other) const
		{
			return std::tie(position, normal, texcoord) == std::tie(other.position, other.normal, other.texcoord);
		}

		bool operator!=(const Vertex_accessor_set& other) const
		{
			return std::tie(position, normal, texcoord) != std::tie(other.position, other.normal, other.texcoord);
		}
	};
}

template <>
struct std::hash<VKLIB_HPP_NAMESPACE::io::mesh::gltf::Vertex_accessor_set>
{
	size_t operator()(const VKLIB_HPP_NAMESPACE::io::mesh::gltf::Vertex_accessor_set& set) const
	{
		size_t h1 = std::hash<int>{}(set.position);
		size_t h2 = std::hash<int>{}(set.normal);
		size_t h3 = std::hash<int>{}(set.texcoord);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
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

		std::vector<Buffer>         staging_buffers;
		std::vector<Command_buffer> command_buffers;
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

	struct Material
	{
		size_t    albedo_idx, metal_roughness_idx, normal_idx, emissive_idx, occlusion_idx;
		bool      double_sided;
		glm::vec3 emissive_strength;

		struct Mat_params
		{
			glm::vec3 emissive_strength;
		};

		Descriptor_set descriptor_set, albedo_only_descriptor_set;
		Buffer         uniform_buffer;
	};

	using vertex = io::mesh::Mesh_vertex;

	struct Node
	{
		uint32_t              mesh_idx;
		glm::mat4             transformation = glm::mat4(1.0);
		std::vector<uint32_t> children;
	};

	struct Sized_buffer
	{
		Buffer   buffer;
		uint32_t count;
	};

	struct Primitive
	{
		uint32_t  material_idx, vertices_idx;
		glm::vec3 min, max;
	};

	struct Mesh
	{
		std::vector<Primitive> primitives;
	};

	struct Scene
	{
		std::vector<uint32_t> nodes;
	};

	enum class Load_stage
	{
		Uninitialized = 0,
		Tinygltf_loading,
		Load_material,
		Load_mesh,
		Transfer,
		Finished,
		Error = -1
	};

	class Model
	{
	  public:

		std::vector<Texture>      textures;
		std::vector<Texture_view> texture_views;
		std::vector<Node>         nodes;
		std::vector<Material>     materials;
		std::vector<Mesh>         meshes;
		std::vector<Sized_buffer> vertex_buffers;
		std::vector<Scene>        scenes;

		Descriptor_pool material_descriptor_pool;

		void load_gltf_ascii(
			Loader_context&          loader_context,
			const std::string&       path,
			std::atomic<Load_stage>* stage_info = nullptr
		);

		void load_gltf_bin(
			Loader_context&          loader_context,
			const std::string&       path,
			std::atomic<Load_stage>* stage_info = nullptr
		);

		void load_gltf_memory(
			Loader_context&          loader_context,
			std::span<uint8_t>       data,
			std::atomic<Load_stage>* stage_info = nullptr
		);

	  private:

		struct Internal_context
		{
			Loader_context&                                   context;
			std::unordered_map<Vertex_accessor_set, uint32_t> vertex_map;
			std::unordered_map<uint32_t, uint32_t>            index_map;
			std::unordered_map<uint32_t, uint32_t>            texture_map;
		};

		void load(
			Loader_context&          loader_context,
			const tinygltf::Model&   gltf_model,
			std::atomic<Load_stage>* stage_info = nullptr
		);
		void load_material(Internal_context& loader_context, const tinygltf::Model& gltf_model);
		void load_scene(const tinygltf::Model& gltf_model);
		void load_meshes(Internal_context& loader_context, const tinygltf::Model& gltf_model);

		void create_descriptor_pool(const Internal_context& context);

		Primitive parse_primitive(
			const tinygltf::Model&     model,
			const tinygltf::Primitive& primitive,
			Loader_context&            loader_context
		);

		Node parse_node(const tinygltf::Model& model, uint32_t idx, const glm::mat4& transformation);
	};
}
