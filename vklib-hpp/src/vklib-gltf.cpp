#include "vklib-gltf.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
#pragma region "Data Parser"

	namespace data_parser
	{
		template <>
		std::vector<uint32_t> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
		{
			std::vector<uint32_t> dst;

			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			const auto& buffer      = model.buffers[buffer_view.buffer];

			const void* ptr = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
			dst.resize(accessor.count);

			auto assign_data = [&](const auto* ptr)
			{
				std::copy(ptr, ptr + accessor.count, dst.begin());
			};

			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			{
				assign_data((const uint8_t*)ptr);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				assign_data((const uint16_t*)ptr);
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			{
				assign_data((const uint32_t*)ptr);
				break;
			}
			default:
				throw Parse_error(std::format("Data type incompatible with uint32: {}", accessor.componentType));
			}

			return dst;
		}
	}

#pragma endregion

#pragma region "Node"

	glm::mat4 Node_transformation::get_mat4() const
	{
		return glm::translate(glm::mat4(1.0), translation) * glm::scale(glm::mat4(rotation), scale);
	}

	void Node_transformation::set(const tinygltf::Node& node)
	{
		// exists full transformation matrix
		if (node.matrix.size() == 16)
		{
			const auto mat = glm::mat4(glm::make_mat4(node.matrix.data()));

			glm::vec3 skew;
			glm::vec4 perspective;

			// decompose the matrix, ignoreing skew and perspective
			glm::decompose(mat, scale, rotation, translation, skew, perspective);

			// conjungatify rotation
			// rotation = glm::conjugate(rotation);

			return;
		}

		if (node.rotation.size() == 4) rotation = glm::make_quat(node.rotation.data());
		if (node.scale.size() == 3) scale = glm::make_vec3(node.scale.data());
		if (node.translation.size() == 3) translation = glm::make_vec3(node.translation.data());
	}

	void Node::set(const tinygltf::Node& node)
	{
		transformation.set(node);

		mesh_idx = node.mesh;
		name     = node.name;

		children.resize(node.children.size());
		std::copy(node.children.begin(), node.children.end(), children.begin());
	}

#pragma endregion

#pragma region "Model"

	Primitive Model::parse_primitive(
		const tinygltf::Model&     model,
		const tinygltf::Primitive& primitive,
		Mesh_data_context&         mesh_context
	)
	{
		Primitive output_primitive;
		output_primitive.material_idx = primitive.material;

		// Vertices

		auto find_position = primitive.attributes.find("POSITION"), find_normal = primitive.attributes.find("NORMAL"),
			 find_uv = primitive.attributes.find("TEXCOORD_0"), find_tangent = primitive.attributes.find("TANGENT");

		if (auto end = primitive.attributes.end(); find_position == end)
		{
			throw Exception("Missing Required Attribute: POSITION");
		}

		const bool has_indices = primitive.indices >= 0, has_texcoord = find_uv != primitive.attributes.end(),
				   has_normal = find_normal != primitive.attributes.end(), has_tangent = find_tangent != primitive.attributes.end();

		std::vector<uint32_t> indices;
		if (has_indices) indices = data_parser::acquire_accessor<uint32_t>(model, primitive.indices);

		uint32_t vertex_count = has_indices ? indices.size() : model.accessors[find_position->second].count;

		auto parse_data_no_index = [=]<typename T>(std::vector<T>& list, const void* data)
		{
			const auto* dat = reinterpret_cast<const T*>(data);

			if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
			{
				list.resize(list.size() + vertex_count);
				std::copy(dat, dat + vertex_count, list.begin() + list.size() - vertex_count);
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
			{
				for (auto item : Range(vertex_count - 2))
				{
					list.emplace_back(dat[item]);
					list.emplace_back(dat[item + 1]);
					list.emplace_back(dat[item + 2]);
				}
			}
			else
				throw Exception("Unsupported TinyGLTF Vertex Mode");
		};

		auto parse_data_with_index = [=]<typename T>(std::vector<T>& list, const void* data)
		{
			const auto* dat = reinterpret_cast<const T*>(data);

			if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
			{
				for (auto i : Range(vertex_count))
				{
					list.emplace_back(dat[indices[i]]);
				}
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
			{
				for (auto item : Range(vertex_count - 2))
				{
					list.emplace_back(dat[indices[item]]);
					list.emplace_back(dat[indices[item + 1]]);
					list.emplace_back(dat[indices[item + 2]]);
				}
			}
			else
				throw Exception("Unsupported TinyGLTF Vertex Mode");
		};

		auto parse_tangent_data_no_index = [=](std::vector<glm::vec3>& list, const void* data)
		{
			const auto* dat = reinterpret_cast<const glm::vec4*>(data);

			if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
			{
				for (auto item : Range(vertex_count))
				{
					list.emplace_back(dat[item] * dat[item].w);
				}
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
			{
				for (auto item : Range(vertex_count - 2))
				{
					list.emplace_back(dat[item] * dat[item].w);
					list.emplace_back(dat[item + 1] * dat[item + 1].w);
					list.emplace_back(dat[item + 2] * dat[item + 2].w);
				}
			}
			else
				throw Exception("Unsupported TinyGLTF Vertex Mode");
		};

		auto parse_tangent_data_with_index = [=](std::vector<glm::vec3>& list, const void* data)
		{
			const auto* dat = reinterpret_cast<const glm::vec4*>(data);

			if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
			{
				for (auto i : Range(vertex_count))
				{
					list.emplace_back(dat[indices[i]] * dat[indices[i]].w);
				}
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
			{
				for (auto item : Range(vertex_count - 2))
				{
					list.emplace_back(dat[indices[item]] * dat[indices[item]].w);
					list.emplace_back(dat[indices[item + 1]] * dat[indices[item + 1]].w);
					list.emplace_back(dat[indices[item + 2]] * dat[indices[item + 2]].w);
				}
			}
			else
				throw Exception("Unsupported TinyGLTF Vertex Mode");
		};

		// prevent empty data
		if (mesh_context.vec3_data.empty())
		{
			mesh_context.vec3_data.emplace_back();
			mesh_context.vec3_data.back().reserve(Mesh_data_context::max_single_size / sizeof(glm::vec3));
		}
		if (mesh_context.vec2_data.empty())
		{
			mesh_context.vec2_data.emplace_back();
			mesh_context.vec2_data.back().reserve(Mesh_data_context::max_single_size / sizeof(glm::vec2));
		}

		auto find_buffer = [&]<typename T>(std::vector<std::vector<T>>& list) -> size_t
		{
			// Data exceeds single block size
			if (vertex_count * 3 * sizeof(T) > Mesh_data_context::max_single_size)
			{
				// Last block not empty, create a new one
				if (list.back().size() != 0)
				{
					list.emplace_back();
				}

				list.back().reserve(vertex_count * 3);

				return list.size() - 1;
			}

			const auto find = std::find_if(
				list.begin(),
				list.end(),
				[=](const auto& list) -> bool
				{
					return (list.size() + vertex_count * 3) * sizeof(T) <= Mesh_data_context::max_single_size;
				}
			);

			if (find != list.end()) return find - list.begin();

			if ((list.back().size() + vertex_count * 3) * sizeof(T) > Mesh_data_context::max_single_size)
			{
				list.emplace_back();
				list.back().reserve(Mesh_data_context::max_single_size / sizeof(T));
			}

			return list.size() - 1;
		};

		// parse position. returns buffer instead of pointer --> prevent vector reallocation
		const auto [position_buffer, position_offset] = [&]
		{
			const auto buffer_idx = find_buffer(mesh_context.vec3_data);
			auto&      position   = mesh_context.vec3_data[buffer_idx];
			const auto size       = position.size();

			output_primitive.position_buffer = buffer_idx;
			output_primitive.position_offset = size;

			const auto& accessor    = model.accessors[find_position->second];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			const auto& buffer      = model.buffers[buffer_view.buffer];
			const auto* buffer_data
				= reinterpret_cast<const glm::vec3*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);

			output_primitive.min = glm::make_vec3(accessor.minValues.data());
			output_primitive.max = glm::make_vec3(accessor.maxValues.data());

			if (has_indices)
				parse_data_with_index(position, buffer_data);
			else
				parse_data_no_index(position, buffer_data);

			return std::tuple{position, size};
		}();

		// parse normal
		const auto [normal_buffer, normal_offset] = [&]
		{
			const auto buffer_idx = find_buffer(mesh_context.vec3_data);
			auto&      normal     = mesh_context.vec3_data[buffer_idx];
			const auto size       = normal.size();

			output_primitive.normal_buffer = buffer_idx;
			output_primitive.normal_offset = normal.size();

			// has normal, directly parse the data
			if (has_normal)
			{
				const auto& accessor    = model.accessors[find_normal->second];
				const auto& buffer_view = model.bufferViews[accessor.bufferView];
				const auto& buffer      = model.buffers[buffer_view.buffer];
				const auto* buffer_data
					= reinterpret_cast<const glm::vec3*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);

				if (has_indices)
					parse_data_with_index(normal, buffer_data);
				else
					parse_data_no_index(normal, buffer_data);
			}
			else
			{
				// manual generate data
				for (auto idx = 0u; idx < vertex_count; idx += 3)
				{
					const auto [pos0, pos1, pos2] = std::tuple{
						position_buffer[position_offset + idx],
						position_buffer[position_offset + idx + 1],
						position_buffer[position_offset + idx + 2]
					};
					const auto normal_vector = glm::normalize(glm::cross(pos1 - pos0, pos2 - pos0));

					normal.push_back(normal_vector);
					normal.push_back(normal_vector);
					normal.push_back(normal_vector);
				}
			}

			return std::tuple{normal, size};
		}();

		const auto [uv_buffer, uv_offset] = [&]
		{
			const auto buffer_idx = find_buffer(mesh_context.vec2_data);
			auto&      uv         = mesh_context.vec2_data[buffer_idx];
			const auto size       = uv.size();

			output_primitive.uv_buffer = buffer_idx;
			output_primitive.uv_offset = size;

			// has normal, directly parse the data
			if (has_texcoord)
			{
				const auto& accessor    = model.accessors[find_uv->second];
				const auto& buffer_view = model.bufferViews[accessor.bufferView];
				const auto& buffer      = model.buffers[buffer_view.buffer];
				const auto* buffer_data
					= reinterpret_cast<const glm::vec2*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);

				if (has_indices)
					parse_data_with_index(uv, buffer_data);
				else
					parse_data_no_index(uv, buffer_data);
			}
			else
			{
				// manual generate data
				for (auto _ : Range(vertex_count / 3))
				{
					uv.emplace_back(0.0, 0.0);
					uv.emplace_back(0.0, 1.0);
					uv.emplace_back(1.0, 0.0);
				}
			}

			return std::tuple{uv, size};
		}();

		// generate tangent data
		{
			const auto buffer_idx = find_buffer(mesh_context.vec3_data);
			auto&      tangent    = mesh_context.vec3_data[buffer_idx];

			output_primitive.tangent_buffer = buffer_idx;
			output_primitive.tangent_offset = tangent.size();

			// both tangent and normal present (see glTF Specification), directly parse the data
			if (has_tangent && has_normal)
			{
				const auto& accessor    = model.accessors[find_tangent->second];
				const auto& buffer_view = model.bufferViews[accessor.bufferView];
				const auto& buffer      = model.buffers[buffer_view.buffer];
				const auto* buffer_data
					= reinterpret_cast<const glm::vec3*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);

				if (has_indices)
					parse_tangent_data_with_index(tangent, buffer_data);
				else
					parse_tangent_data_no_index(tangent, buffer_data);
			}
			else
			{
				// manual generate tangent
				for (auto idx = 0u; idx < vertex_count; idx += 3)
				{
					const auto [pos0, pos1, pos2] = std::tuple{
						position_buffer[position_offset + idx],
						position_buffer[position_offset + idx + 1],
						position_buffer[position_offset + idx + 2]
					};

					const auto [uv0, uv1, uv2]
						= std::tuple{uv_buffer[uv_offset + idx], uv_buffer[uv_offset + idx + 1], uv_buffer[uv_offset + idx + 2]};

					const auto [tangent0, tangent1, tangent2] = std::tuple{
						algorithm::geometry::vertex_tangent(pos0, pos1, pos2, uv0, uv1, uv2),
						algorithm::geometry::vertex_tangent(pos1, pos0, pos2, uv1, uv0, uv2),
						algorithm::geometry::vertex_tangent(pos2, pos1, pos0, uv2, uv1, uv0)
					};

					// detect NaNs in tangent
					const auto nan_judgement0 = glm::isnan(tangent0) || glm::isinf(tangent0);
					const auto nan_judgement1 = glm::isnan(tangent1) || glm::isinf(tangent1);
					const auto nan_judgement2 = glm::isnan(tangent2) || glm::isinf(tangent2);
					const auto nan_judgement  = nan_judgement0 || nan_judgement1 || nan_judgement2;
					const bool should_discard = nan_judgement.x || nan_judgement.y || nan_judgement.z;

					if (should_discard) [[unlikely]]
					{
						for (auto i : Range(3))
						{
							// if present, swizzle normal vector as the tangent vector
							const auto normal  = normal_buffer[normal_offset + i];
							const auto swizzle = glm::vec3{normal.y, normal.z, normal.x};

							tangent.push_back(swizzle);
						}
					}
					else
					{
						tangent.push_back(tangent0);
						tangent.push_back(tangent1);
						tangent.push_back(tangent2);
					}
				}
			}
		}

		output_primitive.vertex_count = vertex_count;

		return output_primitive;
	}

	void Model::load_all_nodes(const tinygltf::Model& model)
	{
		nodes.reserve(model.nodes.size());

		// iterates over all nodes
		for (const auto& node : model.nodes)
		{
			Node output;
			output.set(node);
			nodes.push_back(output);
		}
	}

	void Model::load_all_materials(Loader_context& context, const tinygltf::Model& gltf_model)
	{
		for (auto i : Range(gltf_model.images.size()))
		{
			const auto& image = gltf_model.images[i];
			if (context.sub_progress) *context.sub_progress = (float)i / gltf_model.images.size();

			Texture tex;
			tex.parse(image, context);
			textures.push_back(tex);
		}

		for (const auto& material : gltf_model.materials)
		{
			Material output_material;

			// Set properties
			output_material.double_sided = material.doubleSided;
			output_material.alpha_cutoff = material.alphaCutoff;
			output_material.name         = material.name;

			output_material.alpha_mode = [=]
			{
				if (material.alphaMode == "OPAQUE") return Alpha_mode::Opaque;
				if (material.alphaMode == "MASK") return Alpha_mode::Mask;
				if (material.alphaMode == "BLEND") return Alpha_mode::Blend;

				throw Exception("Invalid Alpha Mode Property");
			}();

			// Emissive
			output_material.emissive_strength = glm::make_vec3(material.emissiveFactor.data());
			if (material.extensions.contains("KHR_materials_emissive_strength"))
			{
				auto multiplier = material.extensions.at("KHR_materials_emissive_strength")
									  .Get("emissiveStrength")
									  .GetNumberAsDouble();
				output_material.emissive_strength *= multiplier;
			}

			output_material.uniform_buffer = Buffer(
				context.allocator,
				sizeof(Material::Mat_params),
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			const auto mat_params = Material::Mat_params{output_material.emissive_strength, output_material.alpha_cutoff};
			output_material.uniform_buffer << std::to_array({mat_params});

			// Normal Texture
			output_material.normal_idx = texture_views.size();
			if (material.normalTexture.index < 0 || material.normalTexture.index >= (int)gltf_model.textures.size())
			{
				output_material.normal_idx = texture_views.size();

				Texture tex;
				tex.generate(127, 127, 255, 0, context);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views
					.emplace_back(context, textures[gltf_model.textures[material.normalTexture.index].source], vk::ComponentMapping());
			}

			// Albedo
			output_material.albedo_idx = texture_views.size();
			if (material.pbrMetallicRoughness.baseColorTexture.index < 0
				|| material.pbrMetallicRoughness.baseColorTexture.index >= (int)gltf_model.textures.size())
			{
				Texture tex;
				tex.generate(
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.baseColorFactor[0], 0, 255),
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.baseColorFactor[1], 0, 255),
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.baseColorFactor[2], 0, 255),
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.baseColorFactor[3], 0, 255),
					context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context,
					textures[gltf_model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source],
					vk::ComponentMapping()
				);
			}

			// PBR
			output_material.metal_roughness_idx = texture_views.size();
			if (material.pbrMetallicRoughness.metallicRoughnessTexture.index < 0
				|| material.pbrMetallicRoughness.metallicRoughnessTexture.index >= (int)gltf_model.textures.size())
			{

				Texture tex;
				tex.generate(
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.roughnessFactor, 0, 255),
					std::clamp<uint8_t>(255 * material.pbrMetallicRoughness.metallicFactor, 0, 255),
					0,
					0,
					context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context,
					textures[gltf_model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source],
					vk::ComponentMapping(vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB)
				);
			}

			// Occlusion
			output_material.occlusion_idx = texture_views.size();
			if (material.occlusionTexture.index < 0 || material.occlusionTexture.index >= (int)gltf_model.textures.size())
			{
				Texture tex;
				tex.generate(255, 255, 255, 255, context);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context,
					textures[gltf_model.textures[material.occlusionTexture.index].source],
					vk::ComponentMapping()
				);
			}

			// Emissive
			output_material.emissive_idx = texture_views.size();
			if (material.emissiveTexture.index < 0 || material.emissiveTexture.index >= (int)gltf_model.textures.size())
			{
				Texture tex;
				tex.generate(
					std::clamp<int>(255 * material.emissiveFactor[0], 0, 255),
					std::clamp<int>(255 * material.emissiveFactor[1], 0, 255),
					std::clamp<int>(255 * material.emissiveFactor[2], 0, 255),
					255,
					context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context,
					textures[gltf_model.textures[material.emissiveTexture.index].source],
					vk::ComponentMapping()
				);
			}

			materials.push_back(output_material);
		}
	}

	void Model::load_all_scenes(const tinygltf::Model& gltf_model)
	{
		for (const auto& scene : gltf_model.scenes)
		{
			Scene output_scene;

			output_scene.name = scene.name;

			output_scene.nodes.resize(scene.nodes.size());
			std::copy(scene.nodes.begin(), scene.nodes.end(), output_scene.nodes.begin());

			scenes.push_back(output_scene);
		}
	}

	void Model::load_all_meshes(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{
		Mesh_data_context mesh_context;

		for (auto idx : Range(gltf_model.meshes.size()))
		{
			const auto& mesh = gltf_model.meshes[idx];
			if (loader_context.sub_progress) *loader_context.sub_progress = (float)idx / gltf_model.meshes.size();

			Mesh output_mesh;
			output_mesh.name = mesh.name;

			// Parse all primitives
			for (const auto& primitive : mesh.primitives)
			{
				output_mesh.primitives.push_back(parse_primitive(gltf_model, primitive, mesh_context));
			}

			meshes.push_back(output_mesh);
		}

		generate_buffers(loader_context, mesh_context);
	}

	void Model::load_all_animations(const tinygltf::Model& model)
	{
		for (const auto& animation : model.animations)
		{
			Animation output;
			output.load(model, animation);
			animations.push_back(output);
		}
	}

	void Model::generate_buffers(Loader_context& loader_context, const Mesh_data_context& mesh_context)
	{
		const Command_buffer command(loader_context.command_pool);

		command.begin(true);

		for (const auto& buf : mesh_context.vec3_data)
		{
			const auto size = buf.size() * sizeof(glm::vec3);

			const Buffer staging_buffer(
				loader_context.allocator,
				size,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			staging_buffer << buf;

			const Buffer vertex_buffer(
				loader_context.allocator,
				size,
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_GPU_ONLY
			);

			command.copy_buffer(vertex_buffer, staging_buffer, 0, 0, size);

			vec3_buffers.push_back(vertex_buffer);
			loader_context.staging_buffers.push_back(staging_buffer);
		}

		for (const auto& buf : mesh_context.vec2_data)
		{
			const auto size = buf.size() * sizeof(glm::vec2);

			const Buffer staging_buffer(
				loader_context.allocator,
				size,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			staging_buffer << buf;

			const Buffer vertex_buffer(
				loader_context.allocator,
				size,
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_GPU_ONLY
			);

			command.copy_buffer(vertex_buffer, staging_buffer, 0, 0, size);

			vec2_buffers.push_back(vertex_buffer);
			loader_context.staging_buffers.push_back(staging_buffer);
		}

		command.end();

		loader_context.command_buffers.push_back(command);
	}

	void Model::create_descriptor_pool(const Loader_context& context)
	{
		// Descriptor Pool
		std::array<vk::DescriptorPoolSize, 2> pool_size;
		pool_size[0].setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(materials.size() * 6 + 1);
		pool_size[1].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(materials.size() * 2 + 1);

		material_descriptor_pool = Descriptor_pool(context.device, pool_size, materials.size() * 2 + 1);

		for (auto& material : materials)
		{
			// Create Descriptor Set
			auto layouts             = Descriptor_set_layout::to_array({context.texture_descriptor_set_layout});
			auto layouts_albedo_only = Descriptor_set_layout::to_array({context.albedo_only_layout});

			material.descriptor_set = Descriptor_set::create_multiple(context.device, material_descriptor_pool, layouts)[0];

			if (context.albedo_only_layout.is_valid())
				material.albedo_only_descriptor_set
					= Descriptor_set::create_multiple(context.device, material_descriptor_pool, layouts_albedo_only)[0];

			// Write Descriptor Sets
			const auto descriptor_image_write_info = std::to_array<vk::DescriptorImageInfo>(
				{texture_views[material.albedo_idx].descriptor_info(),
				 texture_views[material.metal_roughness_idx].descriptor_info(),
				 texture_views[material.occlusion_idx].descriptor_info(),
				 texture_views[material.normal_idx].descriptor_info(),
				 texture_views[material.emissive_idx].descriptor_info()}
			);

			const auto descriptor_uniform_write_info
				= vk::DescriptorBufferInfo(material.uniform_buffer, 0, sizeof(Material::Mat_params));

			std::array<vk::WriteDescriptorSet, 6> normal_write_info;
			std::array<vk::WriteDescriptorSet, 2> albedo_only_write_info;

			// Albedo
			normal_write_info[0]
				.setDstBinding(0)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 0)
				.setDstSet(material.descriptor_set);
			// Metal-roughness
			normal_write_info[1]
				.setDstBinding(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 1)
				.setDstSet(material.descriptor_set);
			// Occlusion
			normal_write_info[2]
				.setDstBinding(2)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 2)
				.setDstSet(material.descriptor_set);
			// Normal
			normal_write_info[3]
				.setDstBinding(3)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 3)
				.setDstSet(material.descriptor_set);
			// Emissive
			normal_write_info[4]
				.setDstBinding(4)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 4)
				.setDstSet(material.descriptor_set);
			// Material Param
			normal_write_info[5]
				.setDstBinding(5)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setPBufferInfo(&descriptor_uniform_write_info)
				.setDstSet(material.descriptor_set);

			if (context.albedo_only_layout.is_valid())
			{
				albedo_only_write_info[0]
					.setDstBinding(0)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setPImageInfo(descriptor_image_write_info.data() + 0)
					.setDstSet(material.albedo_only_descriptor_set);

				albedo_only_write_info[1]
					.setDstBinding(1)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setPBufferInfo(&descriptor_uniform_write_info)
					.setDstSet(material.albedo_only_descriptor_set);
			}

			const auto combined_write_info = utility::join_array(normal_write_info, albedo_only_write_info);

			context.device->updateDescriptorSets(combined_write_info, {});
		}
	}

	void Model::load(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{

		// parse Materials
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Load_material;
		load_all_materials(loader_context, gltf_model);

		// parse Meshes
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Load_mesh;
		load_all_meshes(loader_context, gltf_model);

		const auto submit_buffer = Command_buffer::to_vector(loader_context.command_buffers);
		loader_context.transfer_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_buffer));

		// parse scenes & nodes
		load_all_scenes(gltf_model);
		load_all_nodes(gltf_model);
		load_all_animations(gltf_model);

		create_descriptor_pool(loader_context);

		loader_context.transfer_queue.waitIdle();
		loader_context.command_buffers.clear();
		loader_context.staging_buffers.clear();
	}

	void Model::load_gltf_bin(Loader_context& loader_context, const std::string& path)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Tinygltf_loading;
		auto result = parser.LoadBinaryFromFile(&gltf_model, &err, &warn, path);

		if (!result)
		{
			if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Error;
			throw Exception(std::format("Load GLTF (Binary Format) Failed: {}", err));
		}

		load(loader_context, gltf_model);
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Finished;
	}

	void Model::load_gltf_ascii(Loader_context& loader_context, const std::string& path)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Tinygltf_loading;
		auto result = parser.LoadASCIIFromFile(&gltf_model, &err, &warn, path);

		if (!result)
		{
			if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Error;
			throw Exception(std::format("Load GLTF (ASCII Format) Failed: {}", err));
		}

		load(loader_context, gltf_model);
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Finished;
	}

	void Model::load_gltf_memory(Loader_context& loader_context, std::span<uint8_t> data)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Tinygltf_loading;
		auto result = parser.LoadBinaryFromMemory(&gltf_model, &err, &warn, data.data(), data.size());

		if (!result)
		{
			if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Error;
			throw Exception(std::format("Load GLTF (Binary Format from MEM) Failed: {}", err));
		}

		load(loader_context, gltf_model);
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Finished;
	}

#pragma endregion

#pragma region "Texture"

	void Texture::parse(const tinygltf::Image& tex, Loader_context& loader_context)
	{
		format = [](uint32_t pixel_type, uint32_t component_count)
		{
			switch (pixel_type)
			{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				switch (component_count)
				{
				case 1:
					return vk::Format::eR8Unorm;
				case 2:
					return vk::Format::eR8G8Unorm;
				case 3:
				case 4:
					return vk::Format::eR8G8B8A8Unorm;
				}
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				switch (component_count)
				{
				case 1:
					return vk::Format::eR16Unorm;
				case 2:
					return vk::Format::eR16G16Unorm;
				case 3:
				case 4:
					return vk::Format::eR16G16B16A16Unorm;
				}
				break;
			}

			// no format available
			throw Exception(std::format("Failed to load texture, pixel_type={}, component={}", pixel_type, component_count));
		}(tex.pixel_type, tex.component);

		Buffer         staging_buffer;
		vk::DeviceSize copy_size [[maybe_unused]];

		/* Create Image */

		width = tex.width, height = tex.height;
		component_count = tex.component == 3 ? 4 : tex.component;
		mipmap_levels   = algorithm::texture::log2_mipmap_level(width, height, 128);
		name            = tex.name;

		image = Image(
			loader_context.allocator,
			vk::ImageType::e2D,
			vk::Extent3D(width, height, 1),
			format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc
				| vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			mipmap_levels
		);

		/* Transfer Data */

		if (tex.component == 3)
		{
			// 3 Component, converts to 4 Component

			// Unsigned Byte
			if (tex.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				std::vector<uint8_t> data(tex.width * tex.height * 4, 255);

				const uint8_t* const raw = tex.image.data();

				for (const auto i : Range(tex.width * tex.height))
				{
					data[i * 4 + 0] = raw[i * 3 + 0];
					data[i * 4 + 1] = raw[i * 3 + 1];
					data[i * 4 + 2] = raw[i * 3 + 2];
				}

				staging_buffer = Buffer(
					loader_context.allocator,
					data.size(),
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::SharingMode::eExclusive,
					VMA_MEMORY_USAGE_CPU_TO_GPU
				);

				copy_size = data.size();

				staging_buffer << data;
			}
			else if (tex.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)  // Unsigned Short
			{
				std::vector<uint16_t> data(tex.width * tex.height * 4, std::numeric_limits<uint16_t>::max());

				const auto* const raw = (const uint16_t*)tex.image.data();

				for (const auto i : Range(tex.width * tex.height))
				{
					data[i * 4 + 0] = raw[i * 3 + 0];
					data[i * 4 + 1] = raw[i * 3 + 1];
					data[i * 4 + 2] = raw[i * 3 + 2];
				}

				staging_buffer = Buffer(
					loader_context.allocator,
					data.size() * sizeof(uint16_t),
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::SharingMode::eExclusive,
					VMA_MEMORY_USAGE_CPU_TO_GPU
				);

				copy_size = data.size() * sizeof(uint16_t);

				staging_buffer << data;
			}
		}
		else
		{
			// non-3 components

			staging_buffer = Buffer(
				loader_context.allocator,
				tex.image.size(),
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			copy_size = tex.image.size();

			staging_buffer << tex.image;
		}

		/* Submit */
		auto command_buffer = Command_buffer(loader_context.command_pool);

		command_buffer.begin();

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{vk::ImageAspectFlagBits::eColor, 0, mipmap_levels, 0, 1}
		);

		const vk::BufferImageCopy
			copy_info(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, vk::Extent3D(width, height, 1));
		command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		algorithm::texture::generate_mipmap(
			command_buffer,
			image,
			width,
			height,
			mipmap_levels,
			0,
			1,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal
		);

		command_buffer.end();

		loader_context.command_buffers.push_back(command_buffer);
		loader_context.staging_buffers.push_back(staging_buffer);
	}

	void Texture::generate(
		uint8_t         value0,
		uint8_t         value1,
		uint8_t         value2,
		uint8_t         value3,
		Loader_context& loader_context
	)
	{
		image = Image(
			loader_context.allocator,
			vk::ImageType::e2D,
			vk::Extent3D(1, 1, 1),
			vk::Format::eR8G8B8A8Unorm,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		auto pixel_data = std::to_array({value0, value1, value2, value3});

		mipmap_levels = 1;
		width = 1, height = 1;
		component_count = 4;
		format          = vk::Format::eR8G8B8A8Unorm;
		name            = std::format("Generated Image ({}, {}, {}, {})", value0, value1, value2, value3);

		const Buffer staging_buffer(
			loader_context.allocator,
			4,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		staging_buffer << pixel_data;

		auto command_buffer = Command_buffer(loader_context.command_pool);

		// Record command buffer
		command_buffer.begin();
		{
			command_buffer.layout_transit(
				image,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal,
				{},
				vk::AccessFlagBits::eTransferWrite,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eTransfer,
				{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			);

			const vk::BufferImageCopy copy_info(
				0,
				0,
				0,
				{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
				{0, 0, 0},
				vk::Extent3D(width, height, 1)
			);
			command_buffer
				.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

			command_buffer.layout_transit(
				image,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eShaderRead,
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eAllGraphics,
				{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			);
		}
		command_buffer.end();

		loader_context.command_buffers.push_back(command_buffer);
		loader_context.staging_buffers.push_back(staging_buffer);
	}

#pragma endregion

#pragma region "Texture View"

	Texture_view::Texture_view(
		const Loader_context&       context,
		const Texture&              texture,
		const tinygltf::Sampler&    sampler,
		const vk::ComponentMapping& components
	)
	{
		view = Image_view(
			context.device,
			texture.image,
			texture.format,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, texture.mipmap_levels, 0, 1},
			components
		);

		static const std::map<uint32_t, vk::SamplerAddressMode> wrap_mode_lut{
			{TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE,   vk::SamplerAddressMode::eClampToEdge   },
			{TINYGLTF_TEXTURE_WRAP_REPEAT,          vk::SamplerAddressMode::eRepeat        },
			{TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, vk::SamplerAddressMode::eMirroredRepeat}
		};

		auto get_wrap_mode = [=](uint32_t gltf_mode)
		{
			const auto find = wrap_mode_lut.find(gltf_mode);
			if (find == wrap_mode_lut.end()) throw Gltf_parse_error(std::format("{} is not a valid wrap mode", gltf_mode));

			return find->second;
		};

		static const std::map<uint32_t, std::tuple<vk::SamplerMipmapMode, vk::Filter>> min_filter_lut{
			{TINYGLTF_TEXTURE_FILTER_LINEAR,                 {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear}  },
			{TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,   {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear}  },
			{TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,  {vk::SamplerMipmapMode::eNearest, vk::Filter::eLinear} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST,                {vk::SamplerMipmapMode::eLinear, vk::Filter::eNearest} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,  {vk::SamplerMipmapMode::eLinear, vk::Filter::eNearest} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, {vk::SamplerMipmapMode::eNearest, vk::Filter::eNearest}},
		};

		auto get_min = [=](uint32_t gltf_mode)
		{
			const auto find = min_filter_lut.find(gltf_mode);
			if (find == min_filter_lut.end()) throw Gltf_parse_error(std::format("{} is not a valid min filter", gltf_mode));

			return find->second;
		};

		static const std::map<uint32_t, vk::Filter> mag_filter_lut{
			{TINYGLTF_TEXTURE_FILTER_LINEAR,  vk::Filter::eLinear },
			{TINYGLTF_TEXTURE_FILTER_NEAREST, vk::Filter::eNearest}
		};

		auto get_mag = [=](uint32_t gltf_mode)
		{
			const auto find = mag_filter_lut.find(gltf_mode);
			if (find == mag_filter_lut.end()) throw Gltf_parse_error(std::format("{} is not a valid mag filter", gltf_mode));

			return find->second;
		};

		const auto [mipmap_mode, min_filter] = get_min(sampler.minFilter);
		const auto mag_filter                = get_mag(sampler.magFilter);

		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(get_wrap_mode(sampler.wrapS))
											 .setAddressModeV(get_wrap_mode(sampler.wrapT))
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(mipmap_mode)
											 .setAnisotropyEnable(context.config.enable_anistropy)
											 .setMaxAnisotropy(context.config.max_anistropy)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(texture.mipmap_levels)
											 .setMinFilter(min_filter)
											 .setMagFilter(mag_filter)
											 .setUnnormalizedCoordinates(false);

		this->sampler = Image_sampler(context.device, sampler_create_info);
	}

	Texture_view::Texture_view(
		const Loader_context&       context,
		const Texture&              texture,
		const vk::ComponentMapping& components
	)
	{
		view = Image_view(
			context.device,
			texture.image,
			texture.format,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, texture.mipmap_levels, 0, 1},
			components
		);

		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
											 .setAnisotropyEnable(context.config.enable_anistropy)
											 .setMaxAnisotropy(context.config.max_anistropy)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(texture.mipmap_levels)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		this->sampler = Image_sampler(context.device, sampler_create_info);
	}

#pragma endregion

#pragma region "Animation Sampler"

	template <>
	glm::vec3 Animation_sampler<glm::vec3>::linear_interpolate(
		const std::set<Keyframe>::const_iterator& first,
		const std::set<Keyframe>::const_iterator& second,
		float                                     time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		return glm::mix(frame1.linear, frame2.linear, (time - frame1.time) / (frame2.time - frame1.time));
	}

	template <>
	glm::quat Animation_sampler<glm::quat>::linear_interpolate(
		const std::set<Keyframe>::const_iterator& first,
		const std::set<Keyframe>::const_iterator& second,
		float                                     time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		return glm::slerp(frame1.linear, frame2.linear, (time - frame1.time) / (frame2.time - frame1.time));
	}

	template <>
	glm::vec3 Animation_sampler<glm::vec3>::cubic_interpolate(
		const std::set<Keyframe>::const_iterator& first,
		const std::set<Keyframe>::const_iterator& second,
		float                                     time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		const float tc = time, td = frame2.time - frame1.time, t = (tc - frame1.time) / td;
		const float t_2 = t * t, t_3 = t_2 * t;

		const glm::vec3 item1 = (2 * t_3 - 3 * t_2 + 1) * frame1.cubic.v, item2 = td * (t_3 - 2 * t_2 + t) * frame1.cubic.b,
						item3 = (-2 * t_3 + 3 * t_2) * frame2.cubic.v, item4 = td * (t_3 - t_2) * frame2.cubic.a;

		return item1 + item2 + item3 + item4;
	}

	template <>
	glm::quat Animation_sampler<glm::quat>::cubic_interpolate(
		const std::set<Keyframe>::const_iterator& first,
		const std::set<Keyframe>::const_iterator& second,
		float                                     time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		const float tc = time, td = frame2.time - frame1.time, t = (tc - frame1.time) / td;
		const float t_2 = t * t, t_3 = t_2 * t;

		const glm::quat item1 = (2 * t_3 - 3 * t_2 + 1) * frame1.cubic.v, item2 = td * (t_3 - 2 * t_2 + t) * frame1.cubic.b,
						item3 = (-2 * t_3 + 3 * t_2) * frame2.cubic.v, item4 = td * (t_3 - t_2) * frame2.cubic.a;

		return glm::normalize(item1 + item2 + item3 + item4);
	}

	template <>
	void Animation_sampler<glm::vec3>::load(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler)
	{
		const static std::unordered_map<std::string, Interpolation_mode> mode_map = {
			{"LINEAR",      Interpolation_mode::Linear      },
			{"STEP",        Interpolation_mode::Step        },
			{"CUBICSPLINE", Interpolation_mode::Cubic_spline}
		};

		if (auto find = mode_map.find(sampler.interpolation); find != mode_map.end())
			mode = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation sampler interpolation mode: {}", sampler.interpolation));

		// check accessor
		if (sampler.input < 0 || sampler.output < 0) throw Animation_parse_error("Invalid animation sampler accessor index");

		// check input accessor type
		if (const auto& accessor = model.accessors[sampler.input];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_SCALAR)
			throw Animation_parse_error("Invalid animation sampler input type");

		// check output accessor type
		if (const auto& accessor = model.accessors[sampler.output];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
			throw Animation_parse_error("Invalid animation sampler output type");

		const auto timestamp_list = data_parser::acquire_accessor<float>(model, sampler.input);
		const auto value_list     = data_parser::acquire_accessor<glm::vec3>(model, sampler.output);

		// check value size
		if (auto target = (mode == Interpolation_mode::Cubic_spline) ? timestamp_list.size() * 3 : timestamp_list.size();
			value_list.size() != target)
			throw Animation_parse_error(
				std::format("Invalid animation sampler data size: {} VEC3, target is {}", value_list.size(), target)
			);

		// store values
		if (mode == Interpolation_mode::Cubic_spline)
			// Cubic spline, 3 values per timestamp
			for (auto i : Range(timestamp_list.size()))
			{
				keyframes
					.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i * 3 + 1], value_list[i * 3], value_list[i * 3 + 2]);
			}
		else
			// Not cubic spline, 1 value per timestamp
			for (auto i : Range(timestamp_list.size()))
			{
				keyframes.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i]);
			}
	}

	template <>
	void Animation_sampler<glm::quat>::load(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler)
	{
		const static std::unordered_map<std::string, Interpolation_mode> mode_map = {
			{"LINEAR",      Interpolation_mode::Linear      },
			{"STEP",        Interpolation_mode::Step        },
			{"CUBICSPLINE", Interpolation_mode::Cubic_spline}
		};

		if (auto find = mode_map.find(sampler.interpolation); find != mode_map.end())
			mode = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation sampler interpolation mode: {}", sampler.interpolation));

		// check accessor
		if (sampler.input < 0 || sampler.output < 0) throw Animation_parse_error("Invalid animation sampler accessor index");

		// check input accessor type
		if (const auto& accessor = model.accessors[sampler.input];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_SCALAR)
			throw Animation_parse_error("Invalid animation sampler input type");

		// check output accessor type
		if (const auto& accessor = model.accessors[sampler.output];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC4)
			throw Animation_parse_error("Invalid animation sampler output type");

		const auto timestamp_list = data_parser::acquire_accessor<float>(model, sampler.input);
		const auto value_list     = data_parser::acquire_normalized_accessor<glm::quat>(model, sampler.output);

		// check value size
		if (auto target = (mode == Interpolation_mode::Cubic_spline) ? timestamp_list.size() * 3 : timestamp_list.size();
			value_list.size() != target)
			throw Animation_parse_error(
				std::format("Invalid animation sampler data size: {} VEC3, target is {}", value_list.size(), target)
			);

		// store values
		if (mode == Interpolation_mode::Cubic_spline)
			// Cubic spline, 3 values per timestamp
			for (auto i : Range(timestamp_list.size()))
			{
				keyframes
					.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i * 3 + 1], value_list[i * 3], value_list[i * 3 + 2]);
			}
		else
			// Not cubic spline, 1 value per timestamp
			for (auto i : Range(timestamp_list.size()))
			{
				keyframes.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i]);
			}
	}

	std::variant<Animation_sampler<glm::vec3>, Animation_sampler<glm::quat>> load_sampler(
		const tinygltf::Model&            model,
		const tinygltf::AnimationSampler& sampler
	)
	{
		const auto& output_accessor = model.accessors[sampler.output];

		switch (output_accessor.type)
		{
		case TINYGLTF_TYPE_VEC3:
		{
			Animation_sampler<glm::vec3> dst;
			dst.load(model, sampler);
			return dst;
		}

		case TINYGLTF_TYPE_VEC4:
		{
			Animation_sampler<glm::quat> dst;
			dst.load(model, sampler);
			return dst;
		}

		default:
			throw Animation_parse_error("Invalid animation sampler output accessor type");
		}
	}

#pragma endregion

#pragma region "Animation"

	bool Animation_channel::load(const tinygltf::AnimationChannel& animation)
	{
		if (animation.target_node < 0 || animation.sampler < 0) return false;

		node    = animation.target_node;
		sampler = animation.sampler;

		const static std::map<std::string, Animation_target> animation_target_lut = {
			{"translation", Animation_target::Translation},
			{"rotation",    Animation_target::Rotation   },
			{"scale",       Animation_target::Scale      },
			{"weights",     Animation_target::Weights    }
		};

		if (auto find = animation_target_lut.find(animation.target_path); find != animation_target_lut.end())
			target = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation path: {}", animation.target_path));

		return true;
	}

	void Animation::load(const tinygltf::Model& model, const tinygltf::Animation& animation)
	{
		name       = animation.name;
		start_time = std::numeric_limits<float>::max();
		end_time   = -std::numeric_limits<float>::max();

		// parse samplers
		for (const auto& sampler : animation.samplers) samplers.push_back(load_sampler(model, sampler));

		auto get_time = [this](uint32_t idx) -> std::tuple<float, float>
		{
			const auto& mut = samplers[idx];
			if (mut.index() == 0)
			{
				const auto& item = std::get<0>(mut);
				return {item.start_time(), item.end_time()};
			}
			else
			{
				const auto& item = std::get<1>(mut);
				return {item.start_time(), item.end_time()};
			}
		};

		// parse channels
		for (const auto& channel : animation.channels)
		{
			Animation_channel output;
			if (!output.load(channel)) continue;

			auto [start, end] = get_time(output.sampler);
			start_time        = std::min(start_time, start);
			end_time          = std::max(end_time, end);

			channels.push_back(output);
		}
	}

	void Animation::set_transformation(
		float                                              time,
		const std::vector<Node>&                           node_list,
		std::unordered_map<uint32_t, Node_transformation>& map
	) const
	{
		for (const auto& channel : channels)
		{
			const auto& sampler_variant = samplers[channel.sampler];

			// check type
			if ((sampler_variant.index() != 1 && channel.target == Animation_target::Rotation)
				|| (sampler_variant.index() != 0 && channel.target != Animation_target::Rotation))
			{
				throw Animation_error("Mismatch sampler type with channel target");
			}

			// check existence
			auto find = map.find(channel.node);
			if (find == map.end()) find = map.emplace(channel.node, node_list[channel.node].transformation).first;

			switch (channel.target)
			{
			case Animation_target::Translation:
			{
				const auto& sampler      = std::get<0>(sampler_variant);
				find->second.translation = sampler[time];
				break;
			}
			case Animation_target::Rotation:
			{
				const auto& sampler   = std::get<1>(sampler_variant);
				find->second.rotation = sampler[time];
				break;
			}
			case Animation_target::Scale:
			{
				const auto& sampler = std::get<0>(sampler_variant);
				find->second.scale  = sampler[time];
				break;
			}
			default:
				continue;
			}
		}
	}

#pragma endregion
}
