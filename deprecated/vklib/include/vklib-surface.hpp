#pragma once

#include "vklib-base.hpp"
#include "vklib-common.h"
#include "vklib-instance.hpp"
#include "vklib-window.hpp"

namespace vklib
{
	class Surface : public Base_copyable_parented<VkSurfaceKHR, Instance>
	{
	  public:
		using Base_copyable_parented<VkSurfaceKHR, Instance>::Base_copyable_parented;

		static Result<Surface, VkResult> create(const Instance& instance, const Window& window);

		void clean() override;
		~Surface() override { clean(); }
	};
}  // namespace vklib