#include "vklib/core/storage.hpp"

namespace VKLIB_HPP_NAMESPACE
{
#pragma region "Image"

	Image::Image(const Vma_allocator& allocator, VmaMemoryUsage mem_usage, const vk::ImageCreateInfo& create_info)
	{
		const VkImageCreateInfo c_create_info = create_info;

		VkImage       handle;
		VmaAllocation allocation_handle;

		const VmaAllocationCreateInfo alloc_info{.usage = mem_usage};

		auto result = vmaCreateImage(allocator, &c_create_info, &alloc_info, &handle, &allocation_handle, nullptr);
		vk::resultCheck(vk::Result(result), "Can't create image");

		*this = Image({vk::Image(handle), allocation_handle}, allocator);
	}

	Image::Image(
		const Vma_allocator&    allocator,
		vk::ImageType           type,
		vk::Extent3D            extent,
		vk::Format              format,
		vk::ImageTiling         tiling_mode,
		vk::ImageUsageFlags     image_usage,
		VmaMemoryUsage          mem_usage,
		vk::SharingMode         sharing_mode,
		uint32_t                mipmap_levels,
		vk::ImageLayout         initial_layout,
		vk::SampleCountFlagBits samples,
		uint32_t                array_layers,
		vk::ImageCreateFlags    create_flags
	)
	{
		vk::ImageCreateInfo create_info;
		create_info.setImageType(type)
			.setExtent(extent)
			.setFormat(format)
			.setTiling(tiling_mode)
			.setSharingMode(sharing_mode)
			.setMipLevels(mipmap_levels)
			.setInitialLayout(initial_layout)
			.setSamples(samples)
			.setArrayLayers(array_layers)
			.setUsage(image_usage)
			.setFlags(create_flags);

		*this = {allocator, mem_usage, create_info};
	}

	void Image::clean()
	{
		if (is_unique()) vmaDestroyImage(parent(), data->child.data, data->child.alloc_handle);
	}

#pragma endregion

#pragma region "Image View"

	Image_view::Image_view(const Device& device, const vk::ImageViewCreateInfo& create_info)
	{
		const auto handle = device->createImageView(create_info);
		*this             = Image_view(handle, device);
	}

	Image_view::Image_view(
		const Device&                    device,
		vk::Image                        image,
		vk::Format                       format,
		vk::ImageViewType                view_type,
		const vk::ImageSubresourceRange& subresource_range,
		const vk::ComponentMapping&      components
	)
	{
		const auto create_info = vk::ImageViewCreateInfo({}, image, view_type, format, components, subresource_range);

		*this = {device, create_info};
	}

	void Image_view::clean()
	{
		if (is_unique()) parent()->destroyImageView(*this);
	}

#pragma endregion

#pragma region "Image Sampler"

	Image_sampler::Image_sampler(const Device& device, const vk::SamplerCreateInfo& create_info)
	{
		auto handle = device->createSampler(create_info);
		*this       = {handle, device};
	}

	void Image_sampler::clean()
	{
		if (is_unique()) parent()->destroySampler(*this);
	}

#pragma endregion

#pragma region "Buffer"

	Buffer::Buffer(
		const Vma_allocator&        allocator,
		const vk::BufferCreateInfo& create_info,
		VmaMemoryUsage              mem_usage,
		VmaAllocationCreateFlags    mem_flags
	)
	{
		const VkBufferCreateInfo      c_create_info = create_info;
		const VmaAllocationCreateInfo alloc_info{.flags = mem_flags, .usage = mem_usage};

		VkBuffer      handle;
		VmaAllocation alloc_handle;

		const auto result = vmaCreateBuffer(allocator, &c_create_info, &alloc_info, &handle, &alloc_handle, nullptr);
		vk::resultCheck(vk::Result(result), "Can't create buffer");

		*this = Buffer({handle, alloc_handle}, allocator);
	}

	Buffer::Buffer(
		const Vma_allocator&     allocator,
		size_t                   size,
		vk::BufferUsageFlags     usage,
		vk::SharingMode          sharing_mode,
		VmaMemoryUsage           mem_usage,
		VmaAllocationCreateFlags mem_flags
	)
	{
		vk::BufferCreateInfo create_info;
		create_info.setSize(size).setSharingMode(sharing_mode).setUsage(usage);

		*this = {allocator, create_info, mem_usage, mem_flags};
	}

	void Buffer::clean()
	{
		if (is_unique()) vmaDestroyBuffer(parent(), data->child.data, data->child.alloc_handle);
	}

#pragma endregion
}