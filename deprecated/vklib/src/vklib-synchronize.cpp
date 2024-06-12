#include "vklib-synchronize.hpp"

namespace vklib
{
	Result<Semaphore, VkResult> Semaphore::create(const Device& device)
	{
		VkSemaphoreCreateInfo create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		create_info.flags = 0;

		VkSemaphore handle;
		auto        result = vkCreateSemaphore(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Semaphore(handle, device);
	}

	void Semaphore::clean()
	{
		if (should_delete()) vkDestroySemaphore(*parent, *content, parent.allocator_ptr());
	}

	Result<Fence, VkResult> Fence::create(const Device& device, bool signaled_at_create)
	{
		VkFenceCreateInfo create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		create_info.flags = signaled_at_create ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		VkFence handle;
		auto    result = vkCreateFence(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Fence(handle, device);
	}

	VkResult Fence::wait(uint64_t timeout)
	{
		return vkWaitForFences(*parent, 1, content.get(), true, timeout);
	}

	void Fence::wait_multiple(const Device& device, const std::vector<VkFence>& wait_list, uint64_t timeout)
	{
		vkWaitForFences(*device, wait_list.size(), wait_list.data(), true, timeout);
	}

	void Fence::reset()
	{
		vkResetFences(*parent, 1, content.get());
	}

	void Fence::reset_multiple(const Device& device, const std::vector<VkFence>& reset_list)
	{
		vkResetFences(*device, reset_list.size(), reset_list.data());
	}

	void Fence::clean()
	{
		if (should_delete()) vkDestroyFence(*parent, *content, parent.allocator_ptr());
	}
}