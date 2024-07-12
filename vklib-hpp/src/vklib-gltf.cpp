#include "vklib-gltf.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace VKLIB_HPP_NAMESPACE::io::mesh::gltf
{
#pragma region "Data Parser"

	namespace data_parser
	{
		struct Parse_error : public Exception
		{
			Parse_error(const std::string& msg, const std::source_location& loc = std::source_location::current()) :
				Exception(std::format("(General Gltf Data Parser Error) {}", msg), {}, loc)
			{
			}
		};

		template <typename T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <>
		std::vector<uint32_t> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <>
		std::vector<uint16_t> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <typename T>
		concept Glm_float_type = utility::glm_type_check::check_glm_type<T, float>();

		template <typename T>
		concept Glm_u16_type = utility::glm_type_check::check_glm_type<T, uint16_t>();

		// Acquire data in accessor of type T, which is made of arbitrary number of `float` components.
		// `T` must be float or any of the glm genTypes.
		template <Glm_float_type T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		template <Glm_u16_type T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

		// Acquire data in accessor.
		// `T` must be float or any of the glm genTypes.
		// This function provides extra ability to decode normalized data to float according to glTF Spec.
		template <Glm_float_type T>
		std::vector<T> acquire_normalized_accessor(const tinygltf::Model& model, uint32_t accessor_idx);

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

		template <>
		std::vector<uint16_t> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
		{
			std::vector<uint16_t> dst;

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
			default:
				throw Parse_error(std::format("Data type incompatible with uint16: {}", accessor.componentType));
			}

			return dst;
		}

		template <Glm_float_type T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
		{
			std::vector<T> dst;

			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			const auto& buffer      = model.buffers[buffer_view.buffer];

			const auto* ptr = reinterpret_cast<const T*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
			dst.resize(accessor.count);

			if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
				throw Parse_error(std::format("Data type incompatible with float32: {}", accessor.componentType));

			if (buffer_view.byteStride == 0)
			{
				std::copy(ptr, ptr + accessor.count, dst.begin());
			}
			else
			{
				for (auto i : Range(accessor.count))
				{
					const auto* src_ptr = (const T*)((const uint8_t*)ptr + i * buffer_view.byteStride);

					dst[i] = *src_ptr;
				}
			}

			return dst;
		}

		template <Glm_u16_type T>
		std::vector<T> acquire_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
		{
			std::vector<T> dst;

			const auto& accessor    = model.accessors[accessor_idx];
			const auto& buffer_view = model.bufferViews[accessor.bufferView];
			const auto& buffer      = model.buffers[buffer_view.buffer];

			const auto* ptr = reinterpret_cast<const T*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
			dst.resize(accessor.count);

			if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
				&& accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				throw Parse_error(std::format("Data type incompatible with uint16: {}", accessor.componentType));

			if (buffer_view.byteStride == 0)
			{
				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				{
					const auto* dptr = (const uint8_t*)ptr;
					std::copy(dptr, dptr + accessor.count * (sizeof(T) / sizeof(uint8_t)), (uint16_t*)dst.data());
					break;
				}
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					std::copy(ptr, ptr + accessor.count, dst.data());
					break;
				default:
					break;
				}
			}
			else
			{
				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				{
					for (auto i : Range(accessor.count))
					{
						const auto* src_ptr = (const uint8_t*)ptr + buffer_view.byteStride * i;
						auto*       dst_ptr = (uint16_t*)&dst[i];

						for (auto offset : Range(sizeof(T) / sizeof(uint16_t)))
						{
							dst_ptr[offset] = *src_ptr;
						}
					}
					break;
				}
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				{
					for (auto i : Range(accessor.count))
					{
						const auto* src_ptr = reinterpret_cast<const T*>((const uint8_t*)ptr + buffer_view.byteStride * i);

						dst[i] = *src_ptr;
					}
					break;
				}
				default:
					break;
				}
			}

			return dst;
		}

		template <Glm_float_type T>
		std::vector<T> acquire_normalized_accessor(const tinygltf::Model& model, uint32_t accessor_idx)
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

			auto copy_to_dst = [&]<typename Ty>(const Ty* ptr, auto transformation)
			{
				if (buffer_view.byteStride == 0)
				{
					for (auto i : Range(sizeof(T) / sizeof(float) * accessor.count))
						*(reinterpret_cast<float*>(dst.data()) + i) = transformation(ptr[i]);
				}
				else
				{
					for (auto i : Range(accessor.count))
					{
						const auto* src_ptr = (const uint8_t*)ptr + i * buffer_view.byteStride;

						for (auto offset : Range(sizeof(T) / sizeof(float)))
						{
							reinterpret_cast<float*>(dst.data() + i)[offset]
								= transformation(reinterpret_cast<const Ty*>(src_ptr)[offset]);
						}
					}
				}
			};

			switch (accessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
			{
				auto transformation = [](float val) -> float
				{
					return val;
				};
				copy_to_dst((const float*)ptr, transformation);
				break;
			}

			case TINYGLTF_COMPONENT_TYPE_BYTE:
			{
				auto transformation = [](int8_t val) -> float
				{
					return std::max(val / 127.0, -1.0);
				};
				copy_to_dst((const int8_t*)ptr, transformation);
				break;
			}

			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			{
				auto transformation = [](uint8_t val) -> float
				{
					return val / 255.0;
				};
				copy_to_dst((const uint8_t*)ptr, transformation);
				break;
			}

			case TINYGLTF_COMPONENT_TYPE_SHORT:
			{
				auto transformation = [](int16_t val) -> float
				{
					return std::max(val / 32767.0, -1.0);
				};
				copy_to_dst((const int16_t*)ptr, transformation);
				break;
			}

			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				auto transformation = [](uint16_t val) -> float
				{
					return val / 65535.0;
				};
				copy_to_dst((const uint16_t*)ptr, transformation);
				break;
			}
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

		mesh_idx = to_optional(node.mesh);
		name     = node.name;
		skin_idx = to_optional(node.skin);

		children.resize(node.children.size());
		std::copy(node.children.begin(), node.children.end(), children.begin());
	}

#pragma endregion

#pragma region "Model"

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
			output_material.alpha_mode = [=]
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
			if (find == wrap_mode_lut.end())
				throw Gltf_parse_error("Unknown Gltf wrap mode", std::format("{} is not a valid wrap mode", gltf_mode));

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

		auto get_min = [=](uint32_t gltf_mode) -> std::tuple<vk::SamplerMipmapMode, vk::Filter>
		{
			const auto find = min_filter_lut.find(gltf_mode);
			if (find == min_filter_lut.end()) return {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear};

			return find->second;
		};

		static const std::map<uint32_t, vk::Filter> mag_filter_lut{
			{TINYGLTF_TEXTURE_FILTER_LINEAR,  vk::Filter::eLinear },
			{TINYGLTF_TEXTURE_FILTER_NEAREST, vk::Filter::eNearest}
		};

		auto get_mag = [=](uint32_t gltf_mode)
		{
			const auto find = mag_filter_lut.find(gltf_mode);
			if (find == mag_filter_lut.end()) return vk::Filter::eLinear;

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

	Texture_view::Texture_view(
		const Loader_context&       context,
		const std::vector<Texture>& texture_list,
		const tinygltf::Model&      model,
		const tinygltf::Texture&    texture,
		const vk::ComponentMapping& components
	) :
		Texture_view(
			texture.sampler >= 0 ? Texture_view(context, texture_list[texture.source], model.samplers[texture.sampler], components)
								 : Texture_view(context, texture_list[texture.source], components)
		)
	{
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

		node    = to_optional(animation.target_node);
		sampler = to_optional(animation.sampler);

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

			auto [start, end] = get_time(output.sampler.value());
			start_time        = std::min(start_time, start);
			end_time          = std::max(end_time, end);

			channels.push_back(output);
		}
	}

	void Animation::set_transformation(float time, const std::function<Node_transformation&(uint32_t)>& func) const
	{
		for (const auto& channel : channels)
		{
			const auto& sampler_variant = samplers[channel.sampler.value()];

			// check type
			if ((sampler_variant.index() != 1 && channel.target == Animation_target::Rotation)
				|| (sampler_variant.index() != 0 && channel.target != Animation_target::Rotation))
			{
				throw Animation_runtime_error("Sampler Type Mismatch", "Mismatch sampler type with channel target");
			}

			auto& find = func(channel.node.value());

			switch (channel.target)
			{
			case Animation_target::Translation:
			{
				const auto& sampler      = std::get<0>(sampler_variant);
				find.translation         = sampler[time];
				break;
			}
			case Animation_target::Rotation:
			{
				const auto& sampler   = std::get<1>(sampler_variant);
				find.rotation         = sampler[time];
				break;
			}
			case Animation_target::Scale:
			{
				const auto& sampler = std::get<0>(sampler_variant);
				find.scale          = sampler[time];
				break;
			}
			default:
				continue;
			}
		}
	}

#pragma endregion

	Camera::Camera(const tinygltf::Camera& camera) :
		name(camera.name)
	{
		if (camera.type == "perspective")
		{
			is_ortho = false;

			const auto& cam = camera.perspective;

			std::tie(znear, zfar, perspective.yfov, perspective.aspect_ratio)
				= std::tuple(cam.znear, cam.zfar, cam.yfov, cam.aspectRatio);
		}
		else if (camera.type == "orthographic")
		{
			is_ortho = true;

			const auto& cam = camera.orthographic;

			std::tie(znear, zfar, ortho.xmag, ortho.ymag) = std::tuple(cam.znear, cam.zfar, cam.xmag, cam.ymag);
		}
		else
			throw Gltf_spec_violation(
				"Invalid camera mode",
				std::format(R"(Invalid camera mode "{}" found in camera "{}")", camera.type, camera.name),
				"5.12.3. camera.type"
			);
	}
}
