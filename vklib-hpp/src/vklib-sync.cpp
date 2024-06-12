#include "vklib-sync.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	Semaphore::Semaphore(const Device& device, vk::SemaphoreCreateFlags create_flags)
	{
		auto create_info = vk::SemaphoreCreateInfo().setFlags(create_flags);
		auto handle      = device->createSemaphore(create_info);
		*this            = Semaphore(handle, device);
	}

	void Semaphore::clean()
	{
		if (is_unique()) parent()->destroySemaphore(*this);
	}

	Fence::Fence(const Device& device, vk::FenceCreateFlags create_flags)
	{
		auto create_info = vk::FenceCreateInfo().setFlags(create_flags);
		auto handle      = device->createFence(create_info);
		*this            = Fence(handle, device);
	}

	void Fence::clean()
	{
		if (is_unique()) parent()->destroyFence(*this);
	}

	vk::Result Fence::wait(uint64_t timeout) const
	{
		return parent()->waitForFences(to<vk::Fence>(), true, timeout);
	}
}