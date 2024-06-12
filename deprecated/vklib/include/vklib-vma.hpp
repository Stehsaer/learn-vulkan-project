#pragma once
// #define VMA_STATIC_VULKAN_FUNCTIONS 0
// #define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vklib-device.hpp"
#include "vma/vk_mem_alloc.h"

namespace vklib
{
	class Vma_allocator : public Base_copyable_parented<VmaAllocator, Device>
	{
		using Base_copyable_parented<VmaAllocator, Device>::Base_copyable_parented;

	  public:

		static Result<Vma_allocator, VkResult> create(const Physical_device& physical_device, const Device& device);

		void clean() override;
		~Vma_allocator() override;
	};

	struct Vma_allocation_wrapper
	{
		VmaAllocator      allocator;
		VmaAllocation     handle;
		VmaAllocationInfo info;

		Result<void*, VkResult> map_memory() const;
		void                    unmap_memory() const;

		template <bool persistant_mapped = false>
		VkResult transfer(const void* const data, size_t size) const
		{
			if constexpr (persistant_mapped)
			{
				memcpy(info.pMappedData, data, size);
			}
			else
			{
				auto mapped_mem_result = map_memory();
				if (!mapped_mem_result) return mapped_mem_result.err();
				void* mapped_mem = mapped_mem_result.val();
				memcpy(mapped_mem, data, size);
				unmap_memory();
			}

			return VK_SUCCESS;
		}

		template <bool persistant_mapped = false, typename T>
			requires(!std::is_same_v<T, void>)
		VkResult transfer(const T* const data, size_t element_count) const
		{
			return transfer<persistant_mapped>((const void* const)data, sizeof(T) * element_count);
		}

		template <bool persistant_mapped = false, std::ranges::contiguous_range T>
		VkResult transfer(const T& val) const
		{
			return transfer<persistant_mapped>(
				(const void* const)val.data(),
				sizeof(decltype(*val.begin())) * val.size()
			);
		}

		template <bool persistant_mapped = false, typename T>
		VkResult transfer(const T& val) const
		{
			return transfer<persistant_mapped>((const void* const)&val, sizeof(T));
		}
	};
}