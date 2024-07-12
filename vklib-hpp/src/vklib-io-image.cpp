#include "vklib-algorithm.hpp"
#include "vklib-io.hpp"
#include <stb_image.h>

namespace VKLIB_HPP_NAMESPACE::io::images
{
	Stbi_image_data<uint8_t> read_8bit(const std::string& path, int request_channel)
	{
		try
		{
			auto img_data = read(path);
			return read_8bit(img_data, request_channel);
		}
		catch (const Image_exception& e)
		{
			throw[=]
			{
				auto rethrow     = e;
				rethrow.filename = path;
				return rethrow;
			}
			();
		}
	}

	Stbi_image_data<uint8_t> read_8bit(std::span<const uint8_t> file_data, int request_channel)
	{
		int width, height, channels;

		const std::unique_ptr<uint8_t, decltype(&stbi_image_free)> img_data(
			stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, request_channel),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw Image_exception("Failed to read image", "[Binary Input]", reason);
		}

		Stbi_image_data<uint8_t> ret = {
			std::vector<uint8_t>(img_data.get(), img_data.get() + width * height * sizeof(uint8_t) * request_channel),
			width,
			height,
			channels,
			request_channel
		};

		return ret;
	}

	Stbi_image_data<uint16_t> read_16bit(const std::string& path, int request_channel)
	{
		try
		{
			auto img_data = read(path);
			return read_16bit(img_data, request_channel);
		}
		catch (const Image_exception& e)
		{
			throw[=]
			{
				auto rethrow     = e;
				rethrow.filename = path;
				return rethrow;
			}
			();
		}
	}

	Stbi_image_data<uint16_t> read_16bit(std::span<const uint8_t> file_data, int request_channel)
	{
		int width, height, channels;

		const std::unique_ptr<uint16_t, decltype(&stbi_image_free)> img_data(
			stbi_load_16_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, request_channel),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw Image_exception("Failed to read image", "[Binary Input]", reason);
		}

		Stbi_image_data<uint16_t> ret = {
			std::vector<uint16_t>(img_data.get(), img_data.get() + width * height * sizeof(uint16_t) * request_channel),
			width,
			height,
			channels,
			request_channel
		};

		return ret;
	}

	Stbi_image_data<float> read_hdri(const std::string& path)
	{
		try
		{
			auto img_data = read(path);
			return read_hdri(img_data);
		}
		catch (const Image_exception& e)
		{
			throw[=]
			{
				auto rethrow     = e;
				rethrow.filename = path;
				return rethrow;
			}
			();
		}
	}

	Stbi_image_data<float> read_hdri(std::span<const uint8_t> file_data)
	{
		int width, height, channels;

		const std::unique_ptr<float, decltype(&stbi_image_free)> img_data(
			stbi_loadf_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4),
			stbi_image_free
		);

		if (img_data == nullptr)
		{
			const char* const reason = stbi_failure_reason();
			throw Image_exception("Failed to read image", "[Binary Input]", reason);
		}

		Stbi_image_data<float> ret
			= {std::vector<float>(img_data.get(), img_data.get() + width * height * 4), width, height, channels, 4};

		return ret;
	}

	Buffer Stbi_image_utility::load_8bit(
		const Physical_device& physical_device,
		const Vma_allocator&   allocator,
		const Command_buffer&  command_buffer,
		const std::string&     path,
		bool                   generate_mipmap,
		bool                   enable_alpha
	)
	{
		// Detect format

		vk::Format image_format;
		int        target_channel;

		if (enable_alpha)
		{
			image_format = vk::Format::eR8G8B8A8Unorm;
		}
		else
		{
			auto format_properties
				= physical_device.getFormatProperties(vk::Format::eR8G8B8Unorm).optimalTilingFeatures;
			auto target_properties = vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc
								   | vk::FormatFeatureFlagBits::eTransferDst | vk::FormatFeatureFlagBits::eTransferSrc
								   | vk::FormatFeatureFlagBits::eSampledImage;

			if ((format_properties & target_properties) == target_properties)
				image_format = vk::Format::eR8G8B8Unorm;
			else
				image_format = vk::Format::eR8G8B8A8Unorm;
		}

		target_channel = image_format == vk::Format::eR8G8B8Unorm ? 3 : 4;

		// Read Image

		const auto image_data   = io::images::read_8bit(path, target_channel);
		const auto image_extent = vk::Extent3D(image_data.width, image_data.height, 1);
		mipmap_level            = std::floor(std::log2(std::min(image_data.width, image_data.height)));

		// Create Objects

		const Buffer staging_buffer(
			allocator,
			image_data.raw_data.size() * sizeof(uint8_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
				| vk::ImageUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			mipmap_level
		);

		// Upload and copy

		staging_buffer << image_data.raw_data;

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipmap_level, 0, 1)
		);

		vk::BufferImageCopy copy_info;
		copy_info.setBufferImageHeight(0)
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setImageExtent(image_extent)
			.setImageOffset({0, 0, 0})
			.setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

		command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		if (generate_mipmap)
			algorithm::texture::generate_mipmap(command_buffer, image, image_data.width, image_data.height, mipmap_level);

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipmap_level, 0, 1)
		);

		return staging_buffer;
	}

	Buffer Stbi_image_utility::load_16bit(
		const Physical_device& physical_device,
		const Vma_allocator&   allocator,
		const Command_buffer&  command_buffer,
		const std::string&     path,
		bool                   generate_mipmap,
		bool                   enable_alpha

	)
	{
		// Detect format

		vk::Format image_format;
		int        target_channel;

		if (enable_alpha)
		{
			image_format = vk::Format::eR16G16B16A16Unorm;
		}
		else
		{
			auto format_properties
				= physical_device.getFormatProperties(vk::Format::eR16G16B16Unorm).optimalTilingFeatures;
			auto target_properties = vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc
								   | vk::FormatFeatureFlagBits::eTransferDst | vk::FormatFeatureFlagBits::eTransferSrc
								   | vk::FormatFeatureFlagBits::eSampledImage;

			if ((format_properties & target_properties) == target_properties)
				image_format = vk::Format::eR16G16B16Unorm;
			else
				image_format = vk::Format::eR16G16B16A16Unorm;
		}

		target_channel = image_format == vk::Format::eR16G16B16Unorm ? 3 : 4;

		// Read Image

		const auto image_data   = io::images::read_16bit(path, target_channel);
		const auto image_extent = vk::Extent3D(image_data.width, image_data.height, 1);
		mipmap_level            = std::floor(std::log2(std::min(image_data.width, image_data.height)));

		// Create Objects

		const Buffer staging_buffer(
			allocator,
			image_data.raw_data.size() * sizeof(uint16_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
				| vk::ImageUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			mipmap_level
		);

		// Upload and copy

		staging_buffer << image_data.raw_data;

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipmap_level, 0, 1)
		);

		vk::BufferImageCopy copy_info;
		copy_info.setBufferImageHeight(0)
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setImageExtent(image_extent)
			.setImageOffset({0, 0, 0})
			.setImageSubresource(vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

		command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		if (generate_mipmap)
			algorithm::texture::generate_mipmap(command_buffer, image, image_data.width, image_data.height, mipmap_level);

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipmap_level, 0, 1)
		);

		return staging_buffer;
	}

	Buffer Stbi_image_utility::load_hdri(
		const Vma_allocator&  allocator,
		const Command_buffer& command_buffer,
		const std::string&    path
	)
	{
		return load_hdri_internal(allocator, command_buffer, read_hdri(path));
	}

	Buffer Stbi_image_utility::load_hdri(
		const Vma_allocator&     allocator,
		const Command_buffer&    command_buffer,
		std::span<const uint8_t> data
	)
	{
		return load_hdri_internal(allocator, command_buffer, read_hdri(data));
	}

	Buffer Stbi_image_utility::load_hdri_internal(
		const Vma_allocator&                      allocator,
		const Command_buffer&                     command_buffer,
		const io::images::Stbi_image_data<float>& image_data
	)
	{
		const vk::Format image_format = vk::Format::eR16G16B16A16Sfloat;

		const auto            image_extent = vk::Extent3D(image_data.width, image_data.height, 1);
		std::vector<uint16_t> image_converted(image_extent.width * image_extent.height * 4);

		mipmap_level = 1;

		for (auto i : Range(image_data.raw_data.size()))
			image_converted[i] = algorithm::conversion::f32_to_f16_clamped(image_data.raw_data[i]);

		// Create Objects

		const Buffer staging_buffer(
			allocator,
			image_converted.size() * sizeof(uint16_t),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		image = Image(
			allocator,
			vk::ImageType::e2D,
			image_extent,
			image_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		// Upload and copy

		staging_buffer << image_converted;

		command_buffer.layout_transit(
			image,
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

		command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAllGraphics,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);

		return staging_buffer;
	}
}
