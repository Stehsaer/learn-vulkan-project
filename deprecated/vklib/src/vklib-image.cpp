#include "vklib-image.hpp"

// Image
namespace vklib
{
	Result<Image, VkResult> Image::create(
		const Vma_allocator&  allocator,
		VkImageType           type,
		VkExtent3D            extent,
		VkFormat              format,
		VkImageTiling         tiling_mode,
		VkImageUsageFlags     image_usage,
		VmaMemoryUsage        mem_usage,
		VkSharingMode         sharing_mode,
		uint32_t              mipmap_level,
		VkImageLayout         initial_layout,
		VkSampleCountFlagBits samples,
		uint32_t              array_layers
	)
	{
		VkImageCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		create_info.imageType     = type;
		create_info.extent        = extent;
		create_info.format        = format;
		create_info.tiling        = tiling_mode;
		create_info.sharingMode   = sharing_mode;
		create_info.usage         = image_usage;
		create_info.initialLayout = initial_layout;
		create_info.mipLevels     = mipmap_level;
		create_info.arrayLayers   = array_layers;
		create_info.samples       = samples;
		create_info.flags         = 0;

		VmaAllocationCreateInfo vma_alloc_create_info{};
		vma_alloc_create_info.priority = 1.0f;
		vma_alloc_create_info.usage    = mem_usage;
		vma_alloc_create_info.flags    = 0;

		VkImage           image_handle;
		VmaAllocation     vma_allocation;
		VmaAllocationInfo vma_alloc_info;

		auto result = vmaCreateImage(
			*allocator,
			&create_info,
			&vma_alloc_create_info,
			&image_handle,
			&vma_allocation,
			&vma_alloc_info
		);

		if (result != VK_SUCCESS)
			return result;
		else
			return Image(
				{
					image_handle,
					{*allocator, vma_allocation, vma_alloc_info}
            },
				allocator
			);
	}

	Image Image::raw_image(VkImage raw_val)
	{
		return Image(Allocated_image_container(raw_val, Vma_allocation_wrapper(nullptr, nullptr, {})), nullptr, false);
	}

	void Image::clean()
	{
		if (should_delete()) vmaDestroyImage(*parent, content->image, content->allocation.handle);
	}
};  // namespace vklib

// Image View
namespace vklib
{
	Result<Image_view, VkResult> Image_view::create(
		const Device&                  device,
		VkImage                        image,
		VkFormat                       format,
		const VkComponentMapping&      components,
		VkImageViewType                view_type,
		const VkImageSubresourceRange& subresource_range
	)
	{
		VkImageViewCreateInfo create_info = {};
		create_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image                 = image;
		create_info.viewType              = view_type;
		create_info.format                = format;
		create_info.components            = components;
		create_info.subresourceRange      = subresource_range;

		VkImageView view;
		auto        result = vkCreateImageView(*device, &create_info, device.allocator_ptr(), &view);

		if (result != VK_SUCCESS)
			return result;
		else
			return Image_view(view, device);
	}

	void Image_view::clean()
	{
		if (should_delete()) vkDestroyImageView(*parent, *content, parent.allocator_ptr());
	}
}  // namespace vklib

// Image Sampler
namespace vklib
{
	Result<Image_sampler, VkResult> Image_sampler::create(const Device& device, const VkSamplerCreateInfo& sampler_info)
	{
		VkSampler handle;
		auto      result = vkCreateSampler(*device, &sampler_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Image_sampler(handle, device);
	}

	void Image_sampler::clean()
	{
		if (should_delete()) vkDestroySampler(*parent, *content, parent.allocator_ptr());
	}
}  // namespace vklib