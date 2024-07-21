#include "data-accessor.hpp"
#include "vklib-gltf.hpp"

namespace VKLIB_HPP_NAMESPACE::io::gltf
{
	static const void* get_buffer_data(const tinygltf::Model& model, uint32_t accessor_idx)
	{
		const auto& accessor    = model.accessors[accessor_idx];
		const auto& buffer_view = model.bufferViews[accessor.bufferView];
		const auto& buffer      = model.buffers[buffer_view.buffer];
		const auto* buffer_data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;

		return buffer_data;
	}

	Primitive Model::parse_primitive(
		const tinygltf::Model&     model,
		const tinygltf::Primitive& primitive,
		Mesh_data_context&         mesh_context
	)
	{
		Primitive output_primitive;
		output_primitive.material_idx = to_optional(primitive.material);

		// Vertex Attributes
		const auto find_position = primitive.attributes.find("POSITION"), find_normal = primitive.attributes.find("NORMAL"),
				   find_uv = primitive.attributes.find("TEXCOORD_0"), find_tangent = primitive.attributes.find("TANGENT");

		// Skin Attributes
		const auto find_weight = primitive.attributes.find("WEIGHTS_0"), find_joints = primitive.attributes.find("JOINTS_0");

		if (auto end = primitive.attributes.end(); find_position == end)
		{
			output_primitive.enabled = false;
			return output_primitive;
		}

		//[ERR] GLTF Spec: A vertex must contains or NOT contains `WEIGHTS_0` and `JOINTS_0` simutaneously
		if (auto end = primitive.attributes.end(); (find_weight != end) ^ (find_joints != end))
			throw Gltf_spec_violation(
				"Attribute Missing",
				"Missing One of the Attributes: WEIGHTS_0 or JOINTS_0. Gltf specifies: For a given primitive, the number of JOINTS_n "
				"attribute sets MUST be equal to the number of WEIGHTS_n attribute sets.",
				"3.7.3.3. Skinned Mesh Attributes"
			);

		const bool has_indices = primitive.indices >= 0, has_texcoord = find_uv != primitive.attributes.end(),
				   has_normal = find_normal != primitive.attributes.end(), has_tangent = find_tangent != primitive.attributes.end();

		const bool has_skin = find_weight != primitive.attributes.end() && find_joints != primitive.attributes.end();

		// Get indices
		std::vector<uint32_t> indices;
		if (has_indices) indices = data_parser::acquire_accessor<uint32_t>(model, primitive.indices);

		uint32_t vertex_count = has_indices ? indices.size() : model.accessors[find_position->second].count;

		/* Data Parsing Function */

		auto parse_data_no_index = [&]<typename T>(std::vector<T>& list, uint32_t accessor_idx, const T* optional_data = nullptr)
		{
			const auto* dat         = optional_data == nullptr ? (const T*)get_buffer_data(model, accessor_idx) : optional_data;
			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];

			if (optional_data || buffer_view.byteStride == 0)
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					list.resize(list.size() + vertex_count);
					std::copy(dat, dat + vertex_count, list.begin() + list.size() - vertex_count);

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					list.reserve(list.size() + vertex_count);

					for (auto item : Range(vertex_count - 2))
					{
						list.emplace_back(dat[item]);
						list.emplace_back(dat[item + 1]);
						list.emplace_back(dat[item + 2]);
					}

					return;
				}
			}
			else
			{
				list.reserve(list.size() + vertex_count);

				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto idx : Range(vertex_count))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * idx;
						list.emplace_back(*((const T*)src_ptr));
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto idx : Range(vertex_count - 2))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * idx;
						list.emplace_back(*((const T*)src_ptr));

						src_ptr += buffer_view.byteStride;
						list.emplace_back(*((const T*)src_ptr));

						src_ptr += buffer_view.byteStride;
						list.emplace_back(*((const T*)src_ptr));
					}

					return;
				}
			}

			throw Gltf_parse_error(
				"Unsupported TinyGLTF Vertex Mode",
				"The parser only supports Triangle Strip and Triange List by now."
			);
		};

		auto parse_data_with_index = [&]<typename T>(std::vector<T>& list, uint32_t accessor_idx, const T* optional_data = nullptr)
		{
			const auto* dat         = optional_data == nullptr ? (const T*)get_buffer_data(model, accessor_idx) : optional_data;
			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			list.reserve(list.size() + vertex_count);

			if (optional_data || buffer_view.byteStride == 0)
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto i : Range(vertex_count))
					{
						list.emplace_back(dat[indices[i]]);
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto item : Range(vertex_count - 2))
					{
						list.emplace_back(dat[indices[item]]);
						list.emplace_back(dat[indices[item + 1]]);
						list.emplace_back(dat[indices[item + 2]]);
					}

					return;
				}
			}
			else
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto idx : Range(vertex_count))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx];
						list.emplace_back(*((const T*)src_ptr));
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto idx : Range(vertex_count - 2))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx];
						list.emplace_back(*((const T*)src_ptr));

						src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx + 1];
						list.emplace_back(*((const T*)src_ptr));

						src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx + 2];
						list.emplace_back(*((const T*)src_ptr));
					}

					return;
				}
			}

			throw Gltf_parse_error(
				"Unsupported TinyGLTF Vertex Mode",
				"The parser only supports Triangle Strip and Triange List by now."
			);
		};

		auto parse_tangent_data_no_index = [&](std::vector<glm::vec3>& list, uint32_t accessor_idx)
		{
			using T = glm::vec4;

			const auto* dat         = (const T*)get_buffer_data(model, accessor_idx);
			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			list.reserve(list.size() + vertex_count);

			if (buffer_view.byteStride == 0)
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto i : Range(vertex_count))
					{
						list.emplace_back(dat[i] * dat[i].w);
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto item : Range(vertex_count - 2))
					{
						list.emplace_back(dat[item] * dat[item].w);
						list.emplace_back(dat[item + 1] * dat[item].w);
						list.emplace_back(dat[item + 2] * dat[item].w);
					}

					return;
				}
			}
			else
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto idx : Range(vertex_count))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * idx;

						const auto item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto idx : Range(vertex_count - 2))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * idx;

						auto item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);

						src_ptr += buffer_view.byteStride;
						item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);

						src_ptr += buffer_view.byteStride;
						item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);
					}

					return;
				}
			}

			throw Gltf_parse_error(
				"Unsupported TinyGLTF Vertex Mode",
				"The parser only supports Triangle Strip and Triange List by now."
			);
		};

		auto parse_tangent_data_with_index = [&](std::vector<glm::vec3>& list, uint32_t accessor_idx)
		{
			using T = glm::vec4;

			const auto* dat         = (const T*)get_buffer_data(model, accessor_idx);
			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			list.reserve(list.size() + vertex_count);

			if (buffer_view.byteStride == 0)
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto i : Range(vertex_count))
					{
						list.emplace_back(dat[indices[i]] * dat[indices[i]].w);
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto item : Range(vertex_count - 2))
					{
						list.emplace_back(dat[indices[item]] * dat[indices[item]].w);
						list.emplace_back(dat[indices[item + 1]] * dat[indices[item]].w);
						list.emplace_back(dat[indices[item + 2]] * dat[indices[item]].w);
					}

					return;
				}
			}
			else
			{
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					for (auto idx : Range(vertex_count))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx];

						const auto item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);
					}

					return;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					for (auto idx : Range(vertex_count - 2))
					{
						const auto* src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx];

						auto item = *((const T*)src_ptr);
						list.emplace_back(item * item.w);

						src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx + 1];
						item    = *((const T*)src_ptr);
						list.emplace_back(item * item.w);

						src_ptr = (const uint8_t*)dat + buffer_view.byteStride * indices[idx + 2];
						item    = *((const T*)src_ptr);
						list.emplace_back(item * item.w);
					}

					return;
				}
			}

			throw Gltf_parse_error(
				"Unsupported TinyGLTF Vertex Mode",
				"The parser only supports Triangle Strip and Triange List by now."
			);
		};

		auto parse_data = [&]<typename T>(std::vector<T>& list, uint32_t accessor_idx, const T* optional_data = nullptr)
		{
			if (has_indices)
				parse_data_with_index(list, accessor_idx, optional_data);
			else
				parse_data_no_index(list, accessor_idx, optional_data);
		};

		auto find_buffer = [&]<typename T>(std::vector<std::vector<T>>& list) -> size_t
		{
			if (list.empty())
			{
				list.emplace_back();
				list.back().reserve(Mesh_data_context::max_single_size / sizeof(T));
			}

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

		// Position Data, returns (Buffer Index, Buffer Offset) to prevent reallocation of `std::vector`
		const auto [position_buffer, position_offset] = [&]
		{
			const auto buffer_idx = find_buffer(mesh_context.vec3_data);
			auto&      position   = mesh_context.vec3_data[buffer_idx];
			const auto size       = position.size();

			output_primitive.position_buffer = buffer_idx;
			output_primitive.position_offset = size;

			const auto& accessor = model.accessors[find_position->second];

			output_primitive.min = glm::make_vec3(accessor.minValues.data());
			output_primitive.max = glm::make_vec3(accessor.maxValues.data());

			parse_data(position, find_position->second);

			return std::tuple{position, size};
		}();

		// Normal Data
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
				parse_data(normal, find_normal->second);
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

		// UV Data
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
				parse_data(uv, find_uv->second);
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

		// Tangent Data
		{
			const auto buffer_idx = find_buffer(mesh_context.vec3_data);
			auto&      tangent    = mesh_context.vec3_data[buffer_idx];

			output_primitive.tangent_buffer = buffer_idx;
			output_primitive.tangent_offset = tangent.size();

			// both tangent and normal present (see glTF Specification), directly parse the data
			if (has_tangent && has_normal)
			{
				if (has_indices)
					parse_tangent_data_with_index(tangent, find_tangent->second);
				else
					parse_tangent_data_no_index(tangent, find_tangent->second);
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

		// Skin Data
		if (has_skin)
		{
			Primitive_skin skin_info;

			// Parse Joint Data
			{
				const auto buffer_idx   = find_buffer(mesh_context.joint_data);
				auto&      joint_buffer = mesh_context.joint_data[buffer_idx];
				const auto size         = joint_buffer.size();

				skin_info.joint_buffer = buffer_idx;
				skin_info.joint_offset = size;

				const auto joint_data = data_parser::acquire_accessor<glm::u16vec4>(model, find_joints->second);

				parse_data(joint_buffer, 0, joint_data.data());
			}

			// Parse Weight Data
			{
				const auto buffer_idx    = find_buffer(mesh_context.weight_data);
				auto&      weight_buffer = mesh_context.weight_data[buffer_idx];
				const auto size          = weight_buffer.size();

				skin_info.weight_buffer = buffer_idx;
				skin_info.weight_offset = size;

				const auto weight_data = data_parser::acquire_normalized_accessor<glm::vec4>(model, find_weight->second);

				parse_data(weight_buffer, 0, weight_data.data());
			}

			output_primitive.skin = skin_info;
		}

		output_primitive.vertex_count = vertex_count;

		return output_primitive;
	}

	void Model::load_all_nodes(const tinygltf::Model& model)
	{
		nodes.reserve(model.nodes.size());

		// iterates over all nodes
		for (auto idx : Range(model.nodes.size()))
		{
			const auto& node = model.nodes[idx];

			Node output;
			output.set(node);
			nodes.push_back(std::move(output));

			// has camera
			if (node.camera >= 0) cameras[node.camera].node_idx = idx;
		}
	}

	void Model::load_all_images(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{
		for (auto i : Range(gltf_model.images.size()))
		{
			const auto& image = gltf_model.images[i];
			if (loader_context.sub_progress) *loader_context.sub_progress = (float)i / gltf_model.images.size();

			Texture tex;
			tex.parse(image, loader_context);
			textures.push_back(tex);
		}
	}

	void Model::load_all_textures(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{
		for (auto i : Range(gltf_model.textures.size()))
		{
			const auto& texture = gltf_model.textures[i];
			texture_views.emplace_back(loader_context, textures, gltf_model, texture);
		}
	}

	void Model::load_all_materials(Loader_context& context, const tinygltf::Model& gltf_model)
	{
		auto add_material = [&context, gltf_model, this](
								uint32_t&                   self_index,
								const auto&                 tex_info,
								const glm::vec<4, uint8_t>  col,
								const vk::ComponentMapping& mapping = {}
							)
		{
			if (tex_info.index < 0 || tex_info.index > (int)gltf_model.textures.size())
			{
				self_index = texture_views.size();

				Texture tex;
				tex.generate(col.r, col.g, col.b, col.a, context);

				textures.push_back(tex);
				texture_views.emplace_back(context, tex, mapping);
			}
			else
			{
				self_index = tex_info.index;
			}
		};

		for (const auto& material : gltf_model.materials)
		{
			Material output_material;

			// Set properties
			output_material.double_sided = material.doubleSided;
			output_material.name         = material.name;
			output_material.alpha_mode   = [=]
			{
				if (material.alphaMode == "OPAQUE") return Alpha_mode::Opaque;
				if (material.alphaMode == "MASK") return Alpha_mode::Mask;
				if (material.alphaMode == "BLEND") return Alpha_mode::Blend;

				throw Exception("Invalid Alpha Mode Property");
			}();

			// Emissive
			output_material.params = Material::Mat_params{
				glm::make_vec3(material.emissiveFactor.data()),
				{material.pbrMetallicRoughness.roughnessFactor, material.pbrMetallicRoughness.metallicFactor},
				glm::make_vec3(material.pbrMetallicRoughness.baseColorFactor.data()),
				(float)material.alphaCutoff,
				(float)material.normalTexture.scale,
				(float)material.occlusionTexture.strength
			};

			if (material.extensions.contains("KHR_materials_emissive_strength"))
			{
				auto multiplier
					= material.extensions.at("KHR_materials_emissive_strength").Get("emissiveStrength").GetNumberAsDouble();
				output_material.params.emissive_strength = multiplier;
			}

			// Normal Texture
			add_material(output_material.normal_idx, material.normalTexture, {127, 127, 255, 0});

			// Albedo
			add_material(output_material.albedo_idx, material.pbrMetallicRoughness.baseColorTexture, {255, 255, 255, 255});

			// PBR
			add_material(
				output_material.metal_roughness_idx,
				material.pbrMetallicRoughness.metallicRoughnessTexture,
				{255, 255, 255, 255}
			);

			// Occlusion
			add_material(output_material.occlusion_idx, material.occlusionTexture, {255, 255, 255, 255});

			// Emissive
			add_material(output_material.emissive_idx, material.emissiveTexture, {255, 255, 255, 255});

			materials.push_back(std::move(output_material));
		}

		//* Generate Default Material
		{
			Material output_material;

			const struct
			{
				int index = -1;
			} placeholder;

			// Normal Texture
			add_material(output_material.normal_idx, placeholder, {127, 127, 255, 0});

			// Albedo
			add_material(output_material.albedo_idx, placeholder, {255, 255, 255, 255});

			// PBR
			add_material(output_material.metal_roughness_idx, placeholder, {255, 255, 255, 255});

			// Occlusion
			add_material(output_material.occlusion_idx, placeholder, {255, 255, 255, 255});

			// Emissive
			add_material(output_material.emissive_idx, placeholder, {255, 255, 255, 255});

			materials.push_back(std::move(output_material));
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

			scenes.push_back(std::move(output_scene));
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

			meshes.push_back(std::move(output_mesh));
		}

		generate_buffers(loader_context, mesh_context);
	}

	void Model::load_all_animations(const tinygltf::Model& model)
	{
		for (const auto& animation : model.animations)
		{
			Animation output;
			output.load(model, animation);
			animations.push_back(std::move(output));
		}
	}

	void Model::load_all_skins(const tinygltf::Model& model)
	{
		for (const auto& skin : model.skins)
		{
			Skin output_skin;

			output_skin.joints.resize(skin.joints.size());
			std::copy(skin.joints.begin(), skin.joints.end(), output_skin.joints.begin());

			if (skin.inverseBindMatrices > 0)
			{
				output_skin.inverse_bind_matrices = data_parser::acquire_accessor<glm::mat4>(model, skin.inverseBindMatrices);
			}
			else
				output_skin.inverse_bind_matrices = std::vector(skin.joints.size(), glm::mat4(1.0));

			skins.push_back(std::move(output_skin));
		}
	}

	void Model::load_all_cameras(const tinygltf::Model& model)
	{
		cameras.reserve(model.cameras.size());
		for (const auto& camera : model.cameras)
		{
			cameras.emplace_back(camera);
		}
	}

	void Model::generate_buffers(Loader_context& loader_context, const Mesh_data_context& mesh_context)
	{
		const Command_buffer command(loader_context.command_pool);

		command.begin(true);

		auto generate_buffer = [&]<typename T>(const std::vector<std::vector<T>>& buffer, std::vector<Buffer>& dst) -> void
		{
			for (const auto& buf : buffer)
			{
				const auto size = buf.size() * sizeof(T);

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

				loader_context.staging_buffers.push_back(staging_buffer);
				dst.push_back(vertex_buffer);
			}
		};

		generate_buffer(mesh_context.vec3_data, vec3_buffers);
		generate_buffer(mesh_context.vec2_data, vec2_buffers);
		generate_buffer(mesh_context.joint_data, joint_buffers);
		generate_buffer(mesh_context.weight_data, weight_buffers);

		command.end();

		loader_context.command_buffers.push_back(command);
	}

	void Model::load(Loader_context& loader_context, const tinygltf::Model& gltf_model)
	{
		// parse Materials
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Load_material;
		load_all_images(loader_context, gltf_model);
		load_all_textures(loader_context, gltf_model);
		load_all_materials(loader_context, gltf_model);

		// parse Meshes
		if (loader_context.load_stage) *loader_context.load_stage = Load_stage::Load_mesh;
		load_all_meshes(loader_context, gltf_model);

		const auto submit_buffer = Command_buffer::to_vector(loader_context.command_buffers);
		loader_context.transfer_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_buffer));

		// parse scenes & nodes
		load_all_cameras(gltf_model);
		load_all_scenes(gltf_model);
		load_all_nodes(gltf_model);
		load_all_animations(gltf_model);
		load_all_skins(gltf_model);

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
}