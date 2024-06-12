#pragma once

#include "vklib-device.hpp"

namespace vklib
{
	class Descriptor_set_layout : public Base_copyable_parented<VkDescriptorSetLayout, Device>
	{
		using Base_copyable_parented<VkDescriptorSetLayout, Device>::Base_copyable_parented;

	  public:

		static Result<Descriptor_set_layout, VkResult> create(
			const Device&                           device,
			std::span<VkDescriptorSetLayoutBinding> layout_bindings
		);

		void clean() override;

		~Descriptor_set_layout() override;
	};

	class Descriptor_pool : public Base_copyable_parented<VkDescriptorPool, Device>
	{
		using Base_copyable_parented<VkDescriptorPool, Device>::Base_copyable_parented;

	  public:

		static Result<Descriptor_pool, VkResult> create(
			const Device&                         device,
			std::span<const VkDescriptorPoolSize> pool_sizes,
			uint32_t                              max_sets,
			VkDescriptorPoolCreateFlags           flags = 0
		);

		void clean() override;
		~Descriptor_pool() override;
	};

	class Descriptor_set : public Base_copyable_parented<VkDescriptorSet, Descriptor_pool>
	{
		using Base_copyable_parented<VkDescriptorSet, Descriptor_pool>::Base_copyable_parented;

	  public:

		static Result<std::vector<Descriptor_set>, VkResult> allocate_from(
			const Descriptor_pool&           pool,
			uint32_t                         count,
			std::span<VkDescriptorSetLayout> layouts
		);

		void update(std::span<const VkWriteDescriptorSet> write, std::span<const VkCopyDescriptorSet> copy) const;

		void clean() override;
		~Descriptor_set() override { clean(); }
	};

}