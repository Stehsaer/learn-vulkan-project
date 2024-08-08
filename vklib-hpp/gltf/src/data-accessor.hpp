#pragma once
#include "vklib-gltf.hpp"

namespace vklib::io::gltf::data_parser
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
			for (auto i : Iota(accessor.count))
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
				for (auto i : Iota(accessor.count))
				{
					const auto* src_ptr = (const uint8_t*)ptr + buffer_view.byteStride * i;
					auto*       dst_ptr = (uint16_t*)&dst[i];

					for (auto offset : Iota(sizeof(T) / sizeof(uint16_t)))
					{
						dst_ptr[offset] = *src_ptr;
					}
				}
				break;
			}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			{
				for (auto i : Iota(accessor.count))
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
				for (auto i : Iota(sizeof(T) / sizeof(float) * accessor.count))
					*(reinterpret_cast<float*>(dst.data()) + i) = transformation(ptr[i]);
			}
			else
			{
				for (auto i : Iota(accessor.count))
				{
					const auto* src_ptr = (const uint8_t*)ptr + i * buffer_view.byteStride;

					for (auto offset : Iota(sizeof(T) / sizeof(float)))
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
