#include "vklib-gltf.hpp"

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{

#pragma region "Utility"

	struct Vertices_array
	{
		std::vector<vertex> data;
		glm::vec3           min{std::numeric_limits<float>::max()}, max{-std::numeric_limits<float>::max()};
	};

	static Vertices_array load_vertices(
		const tinygltf::Model&       model,
		uint32_t                     draw_mode,
		Vertex_accessor_set          accessor_set,
		const std::vector<uint32_t>* optional_indices = nullptr
	)
	{
		Vertices_array output;

		const bool has_texcoord = accessor_set.texcoord >= 0;

		const auto &position_accessor = model.accessors[accessor_set.position],
				   &normal_accessor   = model.accessors[accessor_set.normal],
				   *uv_accessor       = has_texcoord ? &model.accessors[accessor_set.texcoord] : nullptr;

		auto& data = output.data;
		data.reserve(position_accessor.count);

		// Condition checking
		{
			if (position_accessor.count != normal_accessor.count) throw General_exception("Invalid Accessor Sets");

			if (has_texcoord && (position_accessor.count != uv_accessor->count))
				throw General_exception("Invalid Accessor Sets");

			if (position_accessor.bufferView < 0 || normal_accessor.bufferView < 0)
				throw General_exception("Invalid Accessor.BufferView");

			if (has_texcoord && uv_accessor->count < 0) throw General_exception("Invalid Accessor.BufferView");
		}

		const tinygltf::BufferView &position_buffer_view = model.bufferViews[position_accessor.bufferView],
								   &normal_buffer_view   = model.bufferViews[normal_accessor.bufferView],
								   *uv_buffer_view       = nullptr;

		const void* position_data = model.buffers[position_buffer_view.buffer].data.data()
								  + position_buffer_view.byteOffset + position_accessor.byteOffset;
		const void* normal_data = model.buffers[normal_buffer_view.buffer].data.data() + normal_buffer_view.byteOffset
								+ normal_accessor.byteOffset;

		const void* uv_data = nullptr;

		auto count_stride_func = [](int type) -> size_t
		{
			return type & 0x0f;
		};

		const size_t position_stride = count_stride_func(position_accessor.type),
					 normal_stride   = count_stride_func(normal_accessor.type);
		size_t uv_stride             = 0;

		if (has_texcoord)
		{
			uv_buffer_view = &model.bufferViews[uv_accessor->bufferView];

			uv_data = model.buffers[uv_buffer_view->buffer].data.data() + uv_buffer_view->byteOffset
					+ uv_accessor->byteOffset;

			uv_stride = count_stride_func(uv_accessor->type);
		}

		auto get_data_func = [=](size_t idx) -> std::tuple<const glm::vec3, const glm::vec3, const glm::vec2>
		{
			const auto position = *(const glm::vec3*)((const float*)position_data + idx * position_stride);
			const auto normal   = *(const glm::vec3*)((const float*)normal_data + idx * normal_stride);
			const auto uv
				= has_texcoord ? *(const glm::vec2*)((const float*)uv_data + idx * uv_stride) : glm::vec2(0.0);

			return {position, normal, uv};
		};

		auto calc_tangent_no_index = [&](uint32_t i) -> void
		{
			auto [position0, normal0, uv0] = get_data_func(i);
			auto [position1, normal1, uv1] = get_data_func(i + 1);
			auto [position2, normal2, uv2] = get_data_func(i + 2);

			auto tangent0 = has_texcoord ? algorithm::vertex_tangent(position0, position1, position2, uv0, uv1, uv2)
										 : glm::vec3(1, 0, 0);
			auto tangent1 = has_texcoord ? algorithm::vertex_tangent(position1, position0, position2, uv1, uv0, uv2)
										 : glm::vec3(0, 1, 0);
			auto tangent2 = has_texcoord ? algorithm::vertex_tangent(position2, position0, position1, uv2, uv0, uv1)
										 : glm::vec3(0, 0, 1);

			output.min = glm::min(position0, output.min);
			output.min = glm::min(position1, output.min);
			output.min = glm::min(position2, output.min);

			output.max = glm::max(position0, output.max);
			output.max = glm::max(position1, output.max);
			output.max = glm::max(position2, output.max);

			data.emplace_back(position0, normal0, tangent0, uv0);
			data.emplace_back(position1, normal1, tangent1, uv1);
			data.emplace_back(position2, normal2, tangent2, uv2);
		};

		auto calc_tangent_with_index = [&](uint32_t i) -> void
		{
			auto [position0, normal0, uv0] = get_data_func((*optional_indices)[i]);
			auto [position1, normal1, uv1] = get_data_func((*optional_indices)[i + 1]);
			auto [position2, normal2, uv2] = get_data_func((*optional_indices)[i + 2]);

			auto tangent0 = has_texcoord ? algorithm::vertex_tangent(position0, position1, position2, uv0, uv1, uv2)
										 : glm::vec3(1, 0, 0);
			auto tangent1 = has_texcoord ? algorithm::vertex_tangent(position1, position0, position2, uv1, uv0, uv2)
										 : glm::vec3(0, 1, 0);
			auto tangent2 = has_texcoord ? algorithm::vertex_tangent(position2, position0, position1, uv2, uv0, uv1)
										 : glm::vec3(0, 0, 1);

			output.min = glm::min(position0, output.min);
			output.min = glm::min(position1, output.min);
			output.min = glm::min(position2, output.min);

			output.max = glm::max(position0, output.max);
			output.max = glm::max(position1, output.max);
			output.max = glm::max(position2, output.max);

			data.emplace_back(position0, normal0, tangent0, uv0);
			data.emplace_back(position1, normal1, tangent1, uv1);
			data.emplace_back(position2, normal2, tangent2, uv2);
		};

		auto parse_vertices_no_index = [&](uint32_t draw_mode)
		{
			switch (draw_mode)
			{
			case TINYGLTF_MODE_TRIANGLES:
			{
				for (uint32_t i = 0; i < position_accessor.count; i += 3)
				{
					calc_tangent_no_index(i);
				}
				break;
			}
			case TINYGLTF_MODE_TRIANGLE_STRIP:
			{
				for (uint32_t i = 0; i < position_accessor.count - 3; i++)
				{
					calc_tangent_no_index(i);
				}
			}
			default:
				break;
			}
		};

		auto parse_vertices_with_index = [&](uint32_t draw_mode)
		{
			switch (draw_mode)
			{
			case TINYGLTF_MODE_TRIANGLES:
			{
				for (uint32_t i = 0; i < optional_indices->size(); i += 3)
				{
					calc_tangent_with_index(i);
				}
				break;
			}
			case TINYGLTF_MODE_TRIANGLE_STRIP:
			{
				for (uint32_t i = 0; i < optional_indices->size() - 3; i++)
				{
					calc_tangent_with_index(i);
				}
			}
			default:
				break;
			}
		};

		if (optional_indices == nullptr)
		{
			parse_vertices_no_index(draw_mode);
		}
		else
		{
			parse_vertices_with_index(draw_mode);
		}

		return output;
	}

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

		if (node.scale.size() == 3)
			mat = glm::scale(glm::mat4(1.0), glm::vec3(glm::make_vec3(node.scale.data()))) * mat;

		if (node.translation.size() == 3)
			mat = glm::translate(glm::mat4(1.0), glm::vec3(glm::make_vec3(node.translation.data()))) * mat;

		return mat;
	}

#pragma endregion

#pragma region "Gltf Model"

	Primitive Model::parse_primitive(
		const tinygltf::Model&     model,
		const tinygltf::Primitive& primitive,
		Loader_context&            loader_context
	)
	{
		Primitive output_primitive;
		output_primitive.material_idx = primitive.material;

		// Vertices

		auto find_position = primitive.attributes.find("POSITION"), find_normal = primitive.attributes.find("NORMAL"),
			 find_uv = primitive.attributes.find("TEXCOORD_0");

		if (auto end = primitive.attributes.end(); find_position == end || find_normal == end)
		{
			throw General_exception(std::format(
				"Attribute Missing: {} {}",
				find_position == end ? "POSITION" : "",
				find_normal == end ? "NORMAL" : ""
			));
		}

		const bool has_indices = primitive.indices >= 0, has_texcoord = find_uv != primitive.attributes.end();

		auto accessor_set
			= Vertex_accessor_set(find_position->second, find_normal->second, has_texcoord ? find_uv->second : -1);

		std::vector<uint32_t> indices;

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

		auto command_buffer           = Command_buffer(loader_context.command_pool);
		auto data                     = load_vertices(model, primitive.mode, accessor_set, has_indices ? &indices : nullptr);
		output_primitive.vertices_idx = vertex_buffers.size();

		output_primitive.min = data.min;
		output_primitive.max = data.max;

		Buffer staging_buffer, vertex_buffer;

		vertex_buffer = Buffer(
			loader_context.allocator,
			data.data.size() * sizeof(vertex),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_GPU_ONLY
		);

		staging_buffer = Buffer(
			loader_context.allocator,
			data.data.size() * sizeof(vertex),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		staging_buffer << data.data;

		command_buffer.begin(true);
		command_buffer.copy_buffer(vertex_buffer, staging_buffer, 0, 0, data.data.size() * sizeof(vertex));
		command_buffer.end();

		loader_context.command_buffers.push_back(command_buffer);
		loader_context.staging_buffers.push_back(staging_buffer);

		vertex_buffers.push_back(Sized_buffer(vertex_buffer, data.data.size()));

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

	void Model::load_material(Internal_context& context, const tinygltf::Model& gltf_model)
	{
		for (const auto& image : gltf_model.images)
		{
			Texture tex;
			tex.parse(image, context.context);
			textures.push_back(tex);
		}
		for (const auto& material : gltf_model.materials)
		{
			Material output_material;
			output_material.double_sided = material.doubleSided;

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
				context.context.allocator,
				sizeof(Material::Mat_params),
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);
			output_material.uniform_buffer << std::to_array({output_material.emissive_strength});

			// Normal Texture
			output_material.normal_idx = texture_views.size();
			if (material.normalTexture.index < 0 || material.normalTexture.index >= (int)gltf_model.images.size())
			{
				output_material.normal_idx = texture_views.size();

				Texture tex;
				tex.generate(127, 127, 255, 0, context.context);

				textures.push_back(tex);
				texture_views.emplace_back(context.context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context.context,
					textures[gltf_model.textures[material.normalTexture.index].source],
					vk::ComponentMapping()
				);
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
					context.context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context.context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context.context,
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
					context.context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context.context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context.context,
					textures[gltf_model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source],
					vk::ComponentMapping(vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB)
				);
			}

			// Occlusion
			output_material.occlusion_idx = texture_views.size();
			if (material.occlusionTexture.index < 0 || material.occlusionTexture.index >= (int)gltf_model.images.size())
			{
				Texture tex;
				tex.generate(255, 255, 255, 255, context.context);

				textures.push_back(tex);
				texture_views.emplace_back(context.context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context.context,
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
					context.context
				);

				textures.push_back(tex);
				texture_views.emplace_back(context.context, tex, vk::ComponentMapping());
			}
			else
			{
				texture_views.emplace_back(
					context.context,
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

	void Model::load_meshes(Internal_context& loader_context, const tinygltf::Model& gltf_model)
	{
		for (const auto& mesh : gltf_model.meshes)
		{
			Mesh output_mesh;

			// Parse all primitives
			for (const auto& primitive : mesh.primitives)
			{
				output_mesh.primitives.push_back(parse_primitive(gltf_model, primitive, loader_context.context));
			}

			meshes.push_back(output_mesh);
		}
	}

	void Model::create_descriptor_pool(const Internal_context& context)
	{
		// Descriptor Pool
		std::array<vk::DescriptorPoolSize, 2> pool_size;
		pool_size[0].setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(materials.size() * 6 + 1);
		pool_size[1].setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(materials.size() + 1);

		material_descriptor_pool = Descriptor_pool(context.context.device, pool_size, materials.size() * 2 + 1);

		for (auto& material : materials)
		{
			// Create Descriptor Set
			auto layouts             = Descriptor_set_layout::to_array({context.context.texture_descriptor_set_layout});
			auto layouts_albedo_only = Descriptor_set_layout::to_array({context.context.albedo_only_layout});

			material.descriptor_set
				= Descriptor_set::create_multiple(context.context.device, material_descriptor_pool, layouts)[0];

			if (context.context.albedo_only_layout.is_valid())
				material.albedo_only_descriptor_set = Descriptor_set::create_multiple(
					context.context.device,
					material_descriptor_pool,
					layouts_albedo_only

				)[0];

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

			std::array<vk::WriteDescriptorSet, 7> write_info;

			// Albedo
			write_info[0]
				.setDstBinding(0)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 0)
				.setDstSet(material.descriptor_set);
			if (context.context.albedo_only_layout.is_valid())
				write_info[4]
					.setDstBinding(0)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setPImageInfo(descriptor_image_write_info.data() + 0)
					.setDstSet(material.albedo_only_descriptor_set);
			// Metal-roughness
			write_info[1]
				.setDstBinding(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 1)
				.setDstSet(material.descriptor_set);
			// Occlusion
			write_info[2]
				.setDstBinding(2)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 2)
				.setDstSet(material.descriptor_set);
			// Normal
			write_info[3]
				.setDstBinding(3)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 3)
				.setDstSet(material.descriptor_set);
			// Emissive
			write_info[5]
				.setDstBinding(4)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(descriptor_image_write_info.data() + 4)
				.setDstSet(material.descriptor_set);
			// Material Param
			write_info[6]
				.setDstBinding(5)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setPBufferInfo(&descriptor_uniform_write_info)
				.setDstSet(material.descriptor_set);

			context.context.device->updateDescriptorSets(write_info, {});
		}
	}

	void Model::load(
		Loader_context&          loader_context,
		const tinygltf::Model&   gltf_model,
		std::atomic<Load_stage>* stage_info
	)
	{
		Internal_context context(loader_context);

		// parse Materials
		if (stage_info) *stage_info = Load_stage::Load_material;
		load_material(context, gltf_model);

		// parse Meshes
		if (stage_info) *stage_info = Load_stage::Load_mesh;
		load_meshes(context, gltf_model);

		const auto submit_buffer = Command_buffer::to_vector(loader_context.command_buffers);

		loader_context.transfer_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_buffer));

		// parse nodes
		load_scene(gltf_model);

		if (stage_info) *stage_info = Load_stage::Transfer;
		create_descriptor_pool(context);

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
			throw General_exception(std::format("Load GLTF (Binary Format) Failed: {}", err));
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
			throw General_exception(std::format("Load GLTF (ASCII Format) Failed: {}", err));
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
			throw General_exception(std::format("Load GLTF (Binary Format from MEM) Failed: {}", err));
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
			throw General_exception(
				std::format("Failed to load texture, pixel_type={}, component={}", pixel_type, component_count)
			);
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
