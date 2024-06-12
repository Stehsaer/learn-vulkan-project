#define VMA_IMPLEMENTATION
#include "vklib-vma.hpp"

// Vma_allocator
namespace vklib
{
	Result<Vma_allocator, VkResult> Vma_allocator::create(const Physical_device& physical_device, const Device& device)
	{
		VmaAllocatorCreateInfo create_info = {};
		create_info.device                 = *device;
		create_info.instance               = *(device.get_parent());
		create_info.physicalDevice         = *physical_device;
		// create_info.pVulkanFunctions = &vma_vulkan_functions;
		create_info.vulkanApiVersion = VK_API_VERSION_1_1;

		VmaAllocator handle;
		auto         result = vmaCreateAllocator(&create_info, &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Vma_allocator(handle, device);
	}

	void Vma_allocator::clean()
	{
		if (should_delete()) vmaDestroyAllocator(*content);
	}

	Vma_allocator::~Vma_allocator()
	{
		clean();
	}
}

// Vma_allocation
namespace vklib
{
	Result<void*, VkResult> Vma_allocation_wrapper::map_memory() const
	{
		void* mapped_mem;
		auto  result = vmaMapMemory(allocator, handle, &mapped_mem);
		if (result != VK_SUCCESS)
			return result;
		else
			return mapped_mem;
	}

	void Vma_allocation_wrapper::unmap_memory() const
	{
		vmaUnmapMemory(allocator, handle);
	}
}