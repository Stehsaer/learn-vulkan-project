#pragma once
#include "vklib-env.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	class Semaphore : public Child_resource<vk::Semaphore, Device>
	{
		using Child_resource<vk::Semaphore, Device>::Child_resource;

	  public:

		Semaphore(const Device& device, vk::SemaphoreCreateFlags create_flags = {});

		void clean() override;
		~Semaphore() override { clean(); }
	};

	class Fence : public Child_resource<vk::Fence, Device>
	{
		using Child_resource<vk::Fence, Device>::Child_resource;

		void clean() override;

	  public:

		Fence(const Device& device, vk::FenceCreateFlags create_flags = {});

		vk::Result wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) const;

		~Fence() override { clean(); }
	};
}