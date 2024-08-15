#include "stb_image.h"
#include "vklib/stbi.hpp"
#include <vklib/core/algorithm.hpp>


namespace VKLIB_HPP_NAMESPACE::io::stbi
{
	Stbi_raw_data<uint8_t> load_8bit(const std::string& path, int request_channel)
	{
		try
		{
			auto img_data = read(path);
			return load_8bit(img_data, request_channel);
		}
		catch (const IO_exception& e)
		{
			throw[=]
			{
				auto rethrow = e;
				rethrow.detail += std::format("(File: {})", path);
				return rethrow;
			}
			();
		}
	}

	Stbi_raw_data<uint8_t> load_8bit(std::span<const uint8_t> file_data, int request_channel)
	{
		int width, height, channels;

		const std::unique_ptr<uint8_t, decltype(&stbi_image_free)> img_data(
			stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, request_channel),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw IO_exception("Failed to read image", "[Binary Input]", reason);
		}

		Stbi_raw_data<uint8_t> ret
			= {std::vector<uint8_t>(img_data.get(), img_data.get() + width * height * sizeof(uint8_t) * request_channel),
			   width,
			   height,
			   channels,
			   request_channel};

		return ret;
	}

	Stbi_raw_data<uint16_t> load_16bit(const std::string& path, int request_channel)
	{
		try
		{
			auto img_data = read(path);
			return load_16bit(img_data, request_channel);
		}
		catch (const IO_exception& e)
		{
			throw[=]
			{
				auto rethrow = e;
				rethrow.detail += std::format("(File: {})", path);
				return rethrow;
			}
			();
		}
	}

	Stbi_raw_data<uint16_t> load_16bit(std::span<const uint8_t> file_data, int request_channel)
	{
		int width, height, channels;

		const std::unique_ptr<uint16_t, decltype(&stbi_image_free)> img_data(
			stbi_load_16_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, request_channel),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw IO_exception("Failed to read image", "[Binary Input]", reason);
		}

		Stbi_raw_data<uint16_t> ret
			= {std::vector<uint16_t>(img_data.get(), img_data.get() + width * height * sizeof(uint16_t) * request_channel),
			   width,
			   height,
			   channels,
			   request_channel};

		return ret;
	}

	Stbi_raw_data<float> load_hdri(const std::string& path)
	{
		try
		{
			auto img_data = read(path);
			return load_hdri(img_data);
		}
		catch (const IO_exception& e)
		{
			throw[=]
			{
				auto rethrow = e;
				rethrow.detail += std::format("(File: {})", path);
				return rethrow;
			}
			();
		}
	}

	Stbi_raw_data<float> load_hdri(std::span<const uint8_t> file_data)
	{
		int width, height, channels;

		const std::unique_ptr<float, decltype(&stbi_image_free)> img_data(
			stbi_loadf_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw IO_exception("Failed to read image", reason);
		}

		Stbi_raw_data<float> ret
			= {std::vector<float>(img_data.get(), img_data.get() + width * height * 4), width, height, channels, 4};

		return ret;
	}

	template <>
	Stbi_vulkan_result Stbi_raw_data<float>::to_vulkan(
		const Vma_allocator&  allocator,
		const Command_buffer& command_buffer,
		bool                  generate_mipmap [[maybe_unused]]
	) const
	{
		Stbi_vulkan_result result;

		const vk::Format image_format = vk::Format::eR16G16B16A16Sfloat;
		const auto       image_extent = vk::Extent3D(width, height, 1);

		// Transform 32bit float to 16bit float
		std::vector<uint16_t> image_converted(image_extent.width * image_extent.height * 4);
		for (auto i : Iota(raw_data.size())) image_converted[i] = algorithm::conversion::f32_to_f16_clamped(raw_data[i]);

		// Create Objects

		result.image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		result.staging_buffer = Buffer(
			allocator,
			image_converted.size() * sizeof(uint16_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.mipmap_levels = 1;

		// Upload and copy

		result.staging_buffer << image_converted;

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		vk::BufferImageCopy copy_info;
		copy_info.setBufferImageHeight(0)
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setImageExtent(image_extent)
			.setImageOffset({0, 0, 0})
			.setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

		command_buffer.copy_buffer_to_image(result.image, result.staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		return result;
	}

	template <>
	Stbi_vulkan_result Stbi_raw_data<uint8_t>::to_vulkan(
		const Vma_allocator&  allocator,
		const Command_buffer& command_buffer,
		bool                  generate_mipmap [[maybe_unused]]
	) const
	{
		if (channels != 4)
			throw IO_exception("Unsupported channel count", "The function only supports vulkan conversion of 4-channel images");

		const vk::Format   image_format = vk::Format::eR8G8B8A8Unorm;
		Stbi_vulkan_result result;

		// Read Image
		const auto image_extent = vk::Extent3D(width, height, 1);
		result.mipmap_levels    = std::floor(std::log2(std::min(width, height)));

		// Create Objects

		result.staging_buffer = Buffer(
			allocator,
			raw_data.size() * sizeof(uint8_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			result.mipmap_levels
		);

		// Upload and copy

		result.staging_buffer << raw_data;

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, result.mipmap_levels, 0, 1)
		);

		vk::BufferImageCopy copy_info;
		copy_info.setBufferImageHeight(0)
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setImageExtent(image_extent)
			.setImageOffset({0, 0, 0})
			.setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

		command_buffer.copy_buffer_to_image(result.image, result.staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		if (generate_mipmap) algorithm::texture::generate_mipmap(command_buffer, result.image, width, height, result.mipmap_levels);

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, result.mipmap_levels, 0, 1)
		);

		return result;
	}

	template <>
	Stbi_vulkan_result Stbi_raw_data<uint16_t>::to_vulkan(
		const Vma_allocator&  allocator,
		const Command_buffer& command_buffer,
		bool                  generate_mipmap [[maybe_unused]]
	) const
	{
		if (channels != 4)
			throw IO_exception("Unsupported channel count", "The function only supports vulkan conversion of 4-channel images");

		const vk::Format   image_format = vk::Format::eR16G16B16A16Unorm;
		Stbi_vulkan_result result;

		// Read Image
		const auto image_extent = vk::Extent3D(width, height, 1);
		result.mipmap_levels    = std::floor(std::log2(std::min(width, height)));

		// Create Objects

		result.staging_buffer = Buffer(
			allocator,
			raw_data.size() * sizeof(uint16_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			result.mipmap_levels
		);

		// Upload and copy

		result.staging_buffer << raw_data;

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, result.mipmap_levels, 0, 1)
		);

		vk::BufferImageCopy copy_info;
		copy_info.setBufferImageHeight(0)
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setImageExtent(image_extent)
			.setImageOffset({0, 0, 0})
			.setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

		command_buffer.copy_buffer_to_image(result.image, result.staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		if (generate_mipmap) algorithm::texture::generate_mipmap(command_buffer, result.image, width, height, result.mipmap_levels);

		command_buffer.layout_transit(
			result.image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, result.mipmap_levels, 0, 1)
		);

		return result;
	}
}