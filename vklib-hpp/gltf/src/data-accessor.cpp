#include "data-accessor.hpp"
#include "vklib-gltf.hpp"

namespace VKLIB_HPP_NAMESPACE::io::gltf::data_parser
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
}