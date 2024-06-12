#pragma once

#include "vklib-device.hpp"

namespace vklib
{
	class Semaphore : public Base_copyable_parented<VkSemaphore, Device>
	{
		using Base_copyable_parented<VkSemaphore, Device>::Base_copyable_parented;

	  public:
		static Result<Semaphore, VkResult> create(const Device& device);

		void clean() override;
		~Semaphore() override { clean(); }
	};

	class Fence : public Base_copyable_parented<VkFence, Device>
	{
		using Base_copyable_parented<VkFence, Device>::Base_copyable_parented;

	  public:
		static Result<Fence, VkResult> create(const Device& device, bool signaled_at_create);

		VkResult wait(uint64_t timeout = std::numeric_limits<uint64_t>::max());

		static void wait_multiple(
			const Device&				device,
			const std::vector<VkFence>& wait_list,
			uint64_t					timeout = std::numeric_limits<uint64_t>::max()
		);

		void		reset();
		static void reset_multiple(const Device& device, const std::vector<VkFence>& reset_list);

		void clean() override;
		~Fence() override { clean(); }
	};
}
