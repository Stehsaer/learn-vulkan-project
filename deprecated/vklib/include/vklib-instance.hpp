#pragma once

#include "vklib-base.hpp"

#define VKLIB_ENABLE_CPU_ALLOCATOR false

namespace vklib
{

	std::vector<VkExtensionProperties> get_supported_extensions();
	std::vector<VkLayerProperties>     get_supported_layers();

	class Instance : public Base_copyable<VkInstance>
	{
#if VKLIB_ENABLE_CPU_ALLOCATOR
		std::optional<VkAllocationCallbacks> _allocator = std::nullopt;

		Instance&& set_allocator(std::optional<VkAllocationCallbacks> allocator) &&
		{
			this->_allocator = allocator;
			return std::forward<Instance>(*this);
		}
#endif

	  public:

		using Base_copyable<VkInstance>::Base_copyable;

		static Result<Instance, VkResult> create(
			const VkApplicationInfo&             app_info,
			std::span<const char*>               extensions,
			std::span<const char*>               layers,
			std::optional<VkAllocationCallbacks> allocator = std::nullopt
		);

		void clean() override;
		~Instance() override { clean(); }

		[[nodiscard]] const VkAllocationCallbacks* allocator_ptr() const
		{
#if VKLIB_ENABLE_CPU_ALLOCATOR
			return _allocator ? &_allocator.value() : nullptr;
#else
			return nullptr;
#endif
		}
	};
}  // namespace vklib