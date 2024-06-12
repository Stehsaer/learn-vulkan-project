#pragma once

#include "vklib-base.hpp"
#include "vklib-device.hpp"
#include "vklib-vma.hpp"

namespace vklib
{
	struct Allocated_image_container
	{
		VkImage                image;
		Vma_allocation_wrapper allocation;
	};

	class Image : public Base_copyable_parented<Allocated_image_container, Vma_allocator>
	{
		using Base_copyable_parented<Allocated_image_container, Vma_allocator>::Base_copyable_parented;

	  public:

		static Result<Image, VkResult> create(
			const Vma_allocator&  allocator,
			VkImageType           type,
			VkExtent3D            extent,
			VkFormat              format,
			VkImageTiling         tiling_mode,
			VkImageUsageFlags     image_usage,
			VmaMemoryUsage        mem_usage,
			VkSharingMode         sharing_mode,
			uint32_t              mipmap_levels  = 1,
			VkImageLayout         initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
			VkSampleCountFlagBits samples        = VK_SAMPLE_COUNT_1_BIT,
			uint32_t              array_layers   = 1
		);

		static Image raw_image(VkImage raw_val);

		void clean() override;
		~Image() override { clean(); }
	};

	class Image_view : public Base_copyable_parented<VkImageView, Device>
	{
		using Base_copyable_parented<VkImageView, Device>::Base_copyable_parented;

	  public:

		// Create Image View, accepts raw VkImage as input
		static Result<Image_view, VkResult> create(
			const Device&                  device,
			VkImage                        image,
			VkFormat                       format,
			const VkComponentMapping&      components,
			VkImageViewType                view_type,
			const VkImageSubresourceRange& subresource_range
		);

		static Result<Image_view, VkResult> create(
			const Device&                  device,
			const Image&                   image,
			VkFormat                       format,
			const VkComponentMapping&      components,
			VkImageViewType                view_type,
			const VkImageSubresourceRange& subresource_range
		)
		{
			return create(device, image->image, format, components, view_type, subresource_range);
		}

		void clean() override;
		~Image_view() override { clean(); };
	};

	class Image_sampler : public Base_copyable_parented<VkSampler, Device>
	{
		using Base_copyable_parented<VkSampler, Device>::Base_copyable_parented;

	  public:

		static Result<Image_sampler, VkResult> create(const Device& device, const VkSamplerCreateInfo& sampler_info);

		void clean() override;
		~Image_sampler() override { clean(); }
	};
}