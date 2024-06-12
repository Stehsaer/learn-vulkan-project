#include "vklib-descriptor.hpp"

#define ENABLE_DESCRIPTOR_SET_FREE false

// Descriptor_set_layout
namespace vklib
{
	Result<Descriptor_set_layout, VkResult> Descriptor_set_layout::create(
		const Device&                           device,
		std::span<VkDescriptorSetLayoutBinding> layout_bindings
	)
	{
		VkDescriptorSetLayoutCreateInfo create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};

		create_info.bindingCount = layout_bindings.size();
		create_info.pBindings    = layout_bindings.data();

		VkDescriptorSetLayout handle;

		auto result = vkCreateDescriptorSetLayout(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Descriptor_set_layout(handle, device);
	}

	void Descriptor_set_layout::clean()
	{
		if (should_delete()) vkDestroyDescriptorSetLayout(*parent, *content, parent.allocator_ptr());
	}

	Descriptor_set_layout::~Descriptor_set_layout()
	{
		clean();
	}
}

// Descriptor pool
namespace vklib
{
	Result<Descriptor_pool, VkResult> Descriptor_pool::create(
		const Device&                         device,
		std::span<const VkDescriptorPoolSize> pool_sizes,
		uint32_t                              max_sets,
		VkDescriptorPoolCreateFlags           flags
	)
	{
		VkDescriptorPoolCreateInfo create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		create_info.poolSizeCount = pool_sizes.size();
		create_info.pPoolSizes    = pool_sizes.data();
		create_info.maxSets       = max_sets;
		create_info.flags         = flags;

		VkDescriptorPool handle;
		auto             result = vkCreateDescriptorPool(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Descriptor_pool(handle, device);
	}

	void Descriptor_pool::clean()
	{
		if (should_delete()) vkDestroyDescriptorPool(*parent, *content, parent.allocator_ptr());
	}

	Descriptor_pool::~Descriptor_pool()
	{
		clean();
	}

}

// Descriptor set
namespace vklib
{
	Result<std::vector<Descriptor_set>, VkResult> Descriptor_set::allocate_from(
		const Descriptor_pool&           pool,
		uint32_t                         count,
		std::span<VkDescriptorSetLayout> layouts
	)
	{
		std::vector<VkDescriptorSet> allocated_sets(count);

		VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		alloc_info.descriptorPool     = *pool;
		alloc_info.descriptorSetCount = count;
		alloc_info.pSetLayouts        = layouts.data();

		auto result = vkAllocateDescriptorSets(*(pool.get_parent()), &alloc_info, allocated_sets.data());

		if (result != VK_SUCCESS)
			return result;
		else
		{
			std::vector<Descriptor_set> set(count);
			for (uint32_t i = 0; i < count; i++)
			{
				set[i] = Descriptor_set(allocated_sets[i], pool);
			}

			return set;
		}
	}

	void Descriptor_set::update(std::span<const VkWriteDescriptorSet> write, std::span<const VkCopyDescriptorSet> copy)
		const
	{
		vkUpdateDescriptorSets(*(parent.get_parent()), write.size(), write.data(), copy.size(), copy.data());
	}

	void Descriptor_set::clean()
	{
#if ENABLE_DESCRIPTOR_SET_FREE
		if (should_delete()) vkFreeDescriptorSets(*(parent.get_parent()), *parent, 1, content.get());
#endif
	}
}