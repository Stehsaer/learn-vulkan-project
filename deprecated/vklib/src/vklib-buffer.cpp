#include "vklib-buffer.hpp"

namespace vklib
{
	Result<Buffer, VkResult> Buffer::create(
		const Vma_allocator&     allocator,
		size_t                   type_size,
		size_t                   element_count,
		VkBufferUsageFlags       usage,
		VmaMemoryUsage           mem_usage,
		VmaAllocationCreateFlags alloc_flags
	)
	{
		VkBufferCreateInfo create_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		create_info.size  = type_size * element_count;
		create_info.usage = usage;

		VmaAllocationCreateInfo alloc_create_info = {};
		alloc_create_info.usage                   = mem_usage;
		alloc_create_info.flags                   = alloc_flags;

		VkBuffer          handle;
		VmaAllocation     allocation;
		VmaAllocationInfo alloc_info;

		auto result = vmaCreateBuffer(*allocator, &create_info, &alloc_create_info, &handle, &allocation, &alloc_info);

		if (result != VK_SUCCESS)
			return result;
		else
			return Buffer(
				Allocated_buffer_container{
				  handle,
				  {*allocator, allocation, alloc_info}
            },
				allocator
			);
	}

	void Buffer::clean()
	{
		if (should_delete()) vmaDestroyBuffer(*parent, content->buffer, content->allocation.handle);
	}

	Buffer::~Buffer()
	{
		clean();
	}
}
