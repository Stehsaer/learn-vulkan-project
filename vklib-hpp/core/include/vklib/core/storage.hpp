#pragma once

#include "vklib/core/allocator.hpp"
#include "vklib/core/env.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	class Image : public Vma_allocation<vk::Image>
	{
		using Vma_allocation<vk::Image>::Vma_allocation;

		void clean() override;

	  public:

		Image(const Vma_allocator& allocator, VmaMemoryUsage mem_usage, const vk::ImageCreateInfo& create_info);

		Image(
			const Vma_allocator&    allocator,
			vk::ImageType           type,
			vk::Extent3D            extent,
			vk::Format              format,
			vk::ImageTiling         tiling_mode,
			vk::ImageUsageFlags     image_usage,
			VmaMemoryUsage          mem_usage,
			vk::SharingMode         sharing_mode,
			uint32_t                mipmap_levels  = 1,
			vk::ImageLayout         initial_layout = vk::ImageLayout::eUndefined,
			vk::SampleCountFlagBits samples        = vk::SampleCountFlagBits::e1,
			uint32_t                array_layers   = 1,
			vk::ImageCreateFlags    create_flags   = {}
		);

		~Image() override { clean(); }
	};

	class Image_view : public Child_resource<vk::ImageView, Device>
	{
		using Child_resource<vk::ImageView, Device>::Child_resource;

		void clean() override;

	  public:

		Image_view(const Device& device, const vk::ImageViewCreateInfo& create_info);

		Image_view(
			const Device&                    device,
			vk::Image                        image,
			vk::Format                       format,
			vk::ImageViewType                view_type,
			const vk::ImageSubresourceRange& subresource_range,
			const vk::ComponentMapping&      components = {}
		);

		~Image_view() override { clean(); }
	};

	class Image_sampler : public Child_resource<vk::Sampler, Device>
	{
		using Child_resource<vk::Sampler, Device>::Child_resource;

		void clean() override;

	  public:

		Image_sampler(const Device& device, const vk::SamplerCreateInfo& create_info);

		~Image_sampler() override { clean(); }
	};

	class Buffer : public Vma_allocation<vk::Buffer>
	{
		using Vma_allocation<vk::Buffer>::Vma_allocation;

		void clean() override;

	  public:

		Buffer(
			const Vma_allocator&        allocator,
			const vk::BufferCreateInfo& create_info,
			VmaMemoryUsage              mem_usage,
			VmaAllocationCreateFlags    mem_flags = {}
		);

		Buffer(
			const Vma_allocator&     allocator,
			size_t                   size,
			vk::BufferUsageFlags     usage,
			vk::SharingMode          sharing_mode,
			VmaMemoryUsage           mem_usage,
			VmaAllocationCreateFlags mem_flags = {}
		);

		~Buffer() override { clean(); }

		template <size_t Size>
		inline static std::array<vk::Buffer, Size> to_array(Buffer (&&arr)[Size])
		{
			std::array<vk::Buffer, Size> ret;
			for (auto idx : Iota(Size)) ret[idx] = arr[idx];

			return ret;
		}
	};
}