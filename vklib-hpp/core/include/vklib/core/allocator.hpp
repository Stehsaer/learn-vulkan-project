#pragma once

//* vklib/core/allocator.hpp
//* Contains:
//		- VMA Allocator

#include "vklib/core/env.hpp"
#include <vk_mem_alloc.h>

namespace VKLIB_HPP_NAMESPACE
{
	class Vma_allocator : public Child_resource<VmaAllocator, Device>
	{
		using Child_resource<VmaAllocator, Device>::Child_resource;

		void clean() override;

	  public:

		Vma_allocator(const Physical_device& physical_device, const Device& device, const Instance& instance);

		~Vma_allocator() override { clean(); }
	};

	template <typename T>
	struct Vma_allocation_wrapper
	{
		T             data;
		VmaAllocation alloc_handle;
	};

	template <typename T>
	class Vma_allocation : public Child_resource<Vma_allocation_wrapper<T>, Vma_allocator>
	{
		using Child_resource<Vma_allocation_wrapper<T>, Vma_allocator>::Child_resource;

	  public:

		void* map_memory() const
		{
			void* ptr;
			auto  result = vmaMapMemory(this->parent(), this->data->child.alloc_handle, &ptr);
			vk::resultCheck(vk::Result(result), "VMA map memory failed");

			return ptr;
		}

		void unmap_memory() const { vmaUnmapMemory(this->parent(), this->data->child.alloc_handle); }

		template <std::ranges::contiguous_range Data_T>
		void operator<<(const Data_T& data_span) const
		{
			using Data_type = decltype(*data_span.begin());

			if (data_span.data() == nullptr) return;

			VmaAllocationInfo alloc_info;
			vmaGetAllocationInfo(this->parent(), this->data->child.alloc_handle, &alloc_info);

			if (alloc_info.pMappedData == nullptr)
			{
				auto* mapped_mem = map_memory();
				memcpy(mapped_mem, data_span.data(), data_span.size() * sizeof(Data_type));
				unmap_memory();
			}
			else
			{
				memcpy(alloc_info.pMappedData, data_span.data(), data_span.size() * sizeof(Data_type));
			}
		}

		template <std::ranges::contiguous_range Data_T>
		void operator>>(Data_T& data_span) const
		{
			if (data_span.data() == nullptr) return;

			VmaAllocationInfo alloc_info;
			vmaGetAllocationInfo(this->parent(), this->data->child.alloc_handle, &alloc_info);

			if (alloc_info.pMappedData == nullptr)
			{
				const void* mapped_mem = map_memory();

				VmaAllocationInfo alloc_info;
				vmaGetAllocationInfo(this->parent(), this->data->child.alloc_handle, &alloc_info);

				std::copy(mapped_mem, (uint8_t*)mapped_mem + alloc_info.size, data_span.begin());

				unmap_memory();
			}
			else
			{
				std::copy(alloc_info.pMappedData, (uint8_t*)alloc_info.pMappedData + alloc_info.size, data_span.begin());
			}
		}

		operator T() const { return this->data->child.data; }

		template <typename Dst_T = T>
		Dst_T to() const
		{
			return (Dst_T)(T)(*this);
		}

		virtual void clean()      = 0;
		virtual ~Vma_allocation() = default;
	};
}