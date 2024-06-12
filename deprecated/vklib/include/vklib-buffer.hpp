#pragma once

#include "vklib-device.hpp"
#include "vklib-vma.hpp"

namespace vklib
{
	struct Allocated_buffer_container
	{
		VkBuffer               buffer;
		Vma_allocation_wrapper allocation;
	};

	class Buffer : public Base_copyable_parented<Allocated_buffer_container, Vma_allocator>
	{
		using Base_copyable_parented<Allocated_buffer_container, Vma_allocator>::
			Base_copyable_parented;

	  public:

		/* Create Functions */

		static Result<Buffer, VkResult> create(
			const Vma_allocator&     allocator,
			size_t                   type_size,
			size_t                   element_count,
			VkBufferUsageFlags       usage,
			VmaMemoryUsage           mem_usage   = VMA_MEMORY_USAGE_AUTO,
			VmaAllocationCreateFlags alloc_flags = 0
		);

		template <typename T>
		static Result<Buffer, VkResult> create(
			const Vma_allocator&     allocator,
			size_t                   element_count,
			VkBufferUsageFlags       usage,
			VmaMemoryUsage           mem_usage   = VMA_MEMORY_USAGE_AUTO,
			VmaAllocationCreateFlags alloc_flags = 0
		)
		{
			return create(allocator, sizeof(T), element_count, usage, mem_usage, alloc_flags);
		}

		/* Utility Functions */

		void clean() override;
		~Buffer() override;
	};
}