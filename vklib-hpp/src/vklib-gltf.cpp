#include "vklib-gltf.hpp"

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{

#pragma region "Utility"

	static glm::mat4 node_transform(const tinygltf::Node& node)
	{
		glm::mat4 mat(1.0);

		if (node.matrix.size() == 16)
		{
			mat = glm::make_mat4(node.matrix.data());
		}

		if (node.rotation.size() == 4)
		{
			const glm::quat quaternion = glm::make_quat(node.rotation.data());

			const glm::mat4 trans(quaternion);
			mat = trans * mat;
		}

		if (node.scale.size() == 3) mat = glm::scale(glm::mat4(1.0), glm::vec3(glm::make_vec3(node.scale.data()))) * mat;

		if (node.translation.size() == 3)
			mat = glm::translate(glm::mat4(1.0), glm::vec3(glm::make_vec3(node.translation.data()))) * mat;

		return mat;
	}

#pragma endregion

#pragma region "Gltf Model"

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
			 find_uv = primitive.attributes.find("TEXCOORD_0");

		if (auto end = primitive.attributes.end(); find_position == end)
		{
			throw Exception("Missing Required Attribute: POSITION");
		}

		const bool has_indices = primitive.indices >= 0, has_texcoord = find_uv != primitive.attributes.end(),
				   has_normal = find_normal != primitive.attributes.end();

		std::vector<uint32_t> indices;

		// parse indices data
		if (has_indices)
		{
			const auto& accessor    = model.accessors[primitive.indices];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			const auto& buffer      = model.buffers[buffer_view.buffer];

			const void* ptr = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
			indices.resize(accessor.count);

			switch (accessor.componentType)
			{
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
			{
				const auto* dptr = (const uint16_t*)ptr;
				for (auto i : Range(accessor.count))
				{
					indices[i] = dptr[i];
				}
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
			{
				const auto* dptr = (const uint32_t*)ptr;
				for (auto i : Range(accessor.count))
				{
					indices[i] = dptr[i];
				}
				break;
			}
			}
		}

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

		auto reserve_vec3_data = [&]
		{
			if (vertex_count * 3 * sizeof(glm::vec3) > Mesh_data_context::max_single_size)
			{
				if (mesh_context.vec3_data.back().size() != 0)
				{
					mesh_context.vec3_data.emplace_back();
				}
				mesh_context.vec3_data.back().reserve(vertex_count * 3);

				return;
			}

			if ((mesh_context.vec3_data.back().size() + vertex_count * 3) * sizeof(glm::vec3) > Mesh_data_context::max_single_size)
			{
				mesh_context.vec3_data.emplace_back();
				mesh_context.vec3_data.back().reserve(Mesh_data_context::max_single_size / sizeof(glm::vec3));
			}
		};

		auto reserve_vec2_data = [&]
		{
			if (vertex_count * 3 * sizeof(glm::vec2) > Mesh_data_context::max_single_size)
			{
				if (mesh_context.vec2_data.back().size() != 0)
				{
					mesh_context.vec2_data.emplace_back();
				}
				mesh_context.vec2_data.back().reserve(vertex_count * 3);

				return;
			}

			if ((mesh_context.vec2_data.back().size() + vertex_count * 3) * sizeof(glm::vec2) > Mesh_data_context::max_single_size)
			{
				mesh_context.vec2_data.emplace_back();
				mesh_context.vec2_data.back().reserve(Mesh_data_context::max_single_size / sizeof(glm::vec2));
			}
		};

		// parse position. returns buffer instead of pointer --> prevent vector reallocation
		const auto [position_buffer, position_offset] = [&]
		{
			reserve_vec3_data();

			output_primitive.position_buffer = mesh_context.vec3_data.size() - 1;
			output_primitive.position_offset = mesh_context.vec3_data.back().size();
			auto&       position             = mesh_context.vec3_data.back();
			const auto& size                 = position.size();

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
		{
			reserve_vec3_data();

			output_primitive.normal_buffer = mesh_context.vec3_data.size() - 1;
			output_primitive.normal_offset = mesh_context.vec3_data.back().size();
			auto& normal                   = mesh_context.vec3_data.back();

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
		}

		const auto uv_data = [&]
		{
			reserve_vec2_data();

			output_primitive.uv_buffer = mesh_context.vec2_data.size() - 1;
			output_primitive.uv_offset = mesh_context.vec2_data.back().size();
			auto& uv                   = mesh_context.vec2_data.back();

			const auto* data = uv.data() + uv.size();

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

			return std::span(data, vertex_count);
		}();

		// generate tangent data
		{
			reserve_vec3_data();

			output_primitive.tangent_buffer = mesh_context.vec3_data.size() - 1;
			output_primitive.tangent_offset = mesh_context.vec3_data.back().size();
			auto& tangent                   = mesh_context.vec3_data.back();

			for (auto idx = 0u; idx < vertex_count; idx += 3)
			{
				const auto [pos0, pos1, pos2] = std::tuple{
					position_buffer[position_offset + idx],
					position_buffer[position_offset + idx + 1],
					position_buffer[position_offset + idx + 2]
				};

				const auto [uv0, uv1, uv2]                = std::tuple{uv_data[idx], uv_data[idx + 1], uv_data[idx + 2]};
				const auto [tangent0, tangent1, tangent2] = std::tuple{
					algorithm::vertex_tangent(pos0, pos1, pos2, uv0, uv1, uv2),
					algorithm::vertex_tangent(pos1, pos0, pos2, uv1, uv0, uv2),
					algorithm::vertex_tangent(pos2, pos1, pos0, uv2, uv1, uv0)
				};

				tangent.push_back(tangent0);
				tangent.push_back(tangent1);
				tangent.push_back(tangent2);
			}
		}

		output_primitive.vertex_count = vertex_count;

		return output_primitive;
	}

	Node Model::parse_node(const tinygltf::Model& model, uint32_t idx, const glm::mat4& transformation)
	{
		const auto& gltf_node = model.nodes[idx];

		Node output;
		output.mesh_idx       = gltf_node.mesh;
		output.transformation = transformation * node_transform(gltf_node);

		for (auto idx : gltf_node.children)
		{
			auto child_node = parse_node(model, idx, output.transformation);
			nodes.push_back(child_node);
			output.children.emplace_back(nodes.size() - 1);
		}

		return output;
	}

	void Model::load_material(Loader_context& context, const tinygltf::Model& gltf_model)
	{
		for (const auto& image : gltf_model.images)
		{
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
			if (material.normalTexture.index < 0 || material.normalTexture.index >= (int)gltf_model.images.size())
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
				|| material.pbrMetallicRoughness.baseColorTexture.index >= (int)gltf_model.images.size())
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
				|| material.pbrMetallicRoughness.metallicRoughnessTexture.index >= (int)gltf_model.images.size())
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
			if (material.occlusionTexture.index < 0 || material.occlusionTexture.index >= (int)gltf_model.images.size())
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
			if (material.emissiveTexture.index < 0 || material.emissiveTexture.index >= (int)gltf_model.images.size())
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

	void Model::load_scene(const tinygltf::Model& gltf_model)
	{
		nodes.reserve(gltf_model.nodes.size());
		for (const auto& scene : gltf_model.scenes)
		{
			Scene output_scene;
			for (auto i : scene.nodes)
			{
				auto node = parse_node(gltf_model, i, glm::mat4(1.0));
				nodes.push_back(node);
				output_scene.nodes.emplace_back(nodes.size() - 1);
			}

			scenes.push_back(output_scene);
		}
	}

	void Model::load_meshes(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{
		Mesh_data_context mesh_context;

		for (const auto& mesh : gltf_model.meshes)
		{
			Mesh output_mesh;

			// Parse all primitives
			for (const auto& primitive : mesh.primitives)
			{
				output_mesh.primitives.push_back(parse_primitive(gltf_model, primitive, mesh_context));
			}

			meshes.push_back(output_mesh);
		}

		generate_buffers(loader_context, mesh_context);
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

	void Model::load(
		Loader_context&          loader_context,
		const tinygltf::Model&   gltf_model,
		std::atomic<Load_stage>* stage_info
	)
	{

		// parse Materials
		if (stage_info) *stage_info = Load_stage::Load_material;
		load_material(loader_context, gltf_model);

		// parse Meshes
		if (stage_info) *stage_info = Load_stage::Load_mesh;
		load_meshes(loader_context, gltf_model);

		const auto submit_buffer = Command_buffer::to_vector(loader_context.command_buffers);
		loader_context.transfer_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_buffer));

		// parse nodes
		load_scene(gltf_model);

		if (stage_info) *stage_info = Load_stage::Transfer;
		create_descriptor_pool(loader_context);

		loader_context.transfer_queue.waitIdle();
		loader_context.command_buffers.clear();
		loader_context.staging_buffers.clear();
	}

	void Model::load_gltf_bin(
		Loader_context&          loader_context,
		const std::string&       path,
		std::atomic<Load_stage>* stage_info
	)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (stage_info) *stage_info = Load_stage::Tinygltf_loading;
		auto result = parser.LoadBinaryFromFile(&gltf_model, &err, &warn, path);

		if (!result)
		{
			if (stage_info) *stage_info = Load_stage::Error;
			throw Exception(std::format("Load GLTF (Binary Format) Failed: {}", err));
		}

		load(loader_context, gltf_model, stage_info);
		if (stage_info) *stage_info = Load_stage::Finished;
	}

	void Model::load_gltf_ascii(
		Loader_context&          loader_context,
		const std::string&       path,
		std::atomic<Load_stage>* stage_info
	)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (stage_info) *stage_info = Load_stage::Tinygltf_loading;
		auto result = parser.LoadASCIIFromFile(&gltf_model, &err, &warn, path);

		if (!result)
		{
			if (stage_info) *stage_info = Load_stage::Error;
			throw Exception(std::format("Load GLTF (ASCII Format) Failed: {}", err));
		}

		load(loader_context, gltf_model, stage_info);
		if (stage_info) *stage_info = Load_stage::Finished;
	}

	void Model::load_gltf_memory(
		Loader_context&          loader_context,
		std::span<uint8_t>       data,
		std::atomic<Load_stage>* stage_info
	)
	{
		loader_context.fence = Fence(loader_context.device);

		tinygltf::TinyGLTF parser;
		tinygltf::Model    gltf_model;

		std::string warn, err;

		if (stage_info) *stage_info = Load_stage::Tinygltf_loading;
		auto result = parser.LoadBinaryFromMemory(&gltf_model, &err, &warn, data.data(), data.size());

		if (!result)
		{
			if (stage_info) *stage_info = Load_stage::Error;
			throw Exception(std::format("Load GLTF (Binary Format from MEM) Failed: {}", err));
		}

		load(loader_context, gltf_model, stage_info);
		if (stage_info) *stage_info = Load_stage::Finished;
	}

#pragma endregion

#pragma region "Gltf Texture"

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
				case 4:
					return vk::Format::eR16G16B16A16Unorm;
				}
				break;
			}

			// no format available
			throw Exception(std::format("Failed to load texture, pixel_type={}, component={}", pixel_type, component_count));
		}(tex.pixel_type, tex.component == 3 ? 4 : tex.component);

		Buffer         staging_buffer;
		vk::DeviceSize copy_size [[maybe_unused]];

		/* Create Image */

		width = tex.width, height = tex.height;
		component_count = tex.component == 3 ? 4 : tex.component;
		mipmap_levels   = algorithm::log2_mipmap_level(width, height, 128);
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

		algorithm::generate_mipmap(
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

		const auto sampler_create_info
			= vk::SamplerCreateInfo()
				  .setAddressModeU(vk::SamplerAddressMode::eRepeat)
				  .setAddressModeV(vk::SamplerAddressMode::eRepeat)
				  .setAddressModeW(vk::SamplerAddressMode::eRepeat)
				  .setMipmapMode(vk::SamplerMipmapMode::eLinear)
				  .setAnisotropyEnable(context.physical_device.getFeatures().samplerAnisotropy)
				  .setMaxAnisotropy(std::min(16.0f, context.physical_device.getProperties().limits.maxSamplerAnisotropy)
				  )
				  .setCompareEnable(false)
				  .setMinLod(0)
				  .setMaxLod(texture.mipmap_levels)
				  .setMinFilter(vk::Filter::eLinear)
				  .setMagFilter(vk::Filter::eLinear)
				  .setUnnormalizedCoordinates(false);

		sampler = Image_sampler(context.device, sampler_create_info);
	}

#pragma endregion

}
