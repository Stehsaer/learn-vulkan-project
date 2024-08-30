#define VMA_IMPLEMENTATION
#include "vklib/core/allocator.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	Vma_allocator::Vma_allocator(const Physical_device& physical_device, const Device& device, const Instance& instance)
	{
		const VmaAllocatorCreateInfo create_info{
			.physicalDevice = physical_device,
			.device         = device.to<VkDevice>(),
			.instance       = instance.to<VkInstance>()
		};

		VmaAllocator handle;
		auto         result = vmaCreateAllocator(&create_info, &handle);
		vk::resultCheck(vk::Result(result), "Create VMA allocator failed");

		*this = Vma_allocator(handle, device);
	}

	void Vma_allocator::clean()
	{
		if (is_unique())
		{
			vmaDestroyAllocator(*this);
		}
	}
}