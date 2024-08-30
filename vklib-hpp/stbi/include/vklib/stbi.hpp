// > vklib/stbi.hpp
// ================
// [Author] Stehsaer
// ================
// [Description]
// - Provides wrapped interface to stb_image for reading image files
// - Provides pre-built functions to convert image data to vulkan images

#pragma once

#include <vklib/core/io.hpp>

namespace VKLIB_HPP_NAMESPACE::io::stbi
{
	// Holds vulkan image and staging buffers when converting image data to vulkan images
	struct Stbi_vulkan_result
	{
		int    mipmap_levels = 1;
		Image  image;
		Buffer staging_buffer;
	};

	// Holds raw image data returned from stbi.
	// `Data_T`: Pixel data type; for an 8-bit image, it can be `uint8_t`; for an 16-bit image, it can be `uint16_t`;
	template <typename Data_T>
	struct Stbi_raw_data
	{
		std::vector<Data_T> raw_data;
		int                 width, height;
		int                 original_channels, channels;

		// > Convert raw data to vulkan image.
		// When converting a hdr image, `generate_mipmap` will be ignored.
		// Only works if `channels==4`
		No_discard Stbi_vulkan_result
		to_vulkan(const Vma_allocator& allocator, const Command_buffer& command_buffer, bool generate_mipmap) const;
	};

	// Class of STBI conversion error.
	// Includes: image decoding error
	struct Stbi_conversion_error : public error::Detailed_error
	{
		std::string convert_desc;

		Stbi_conversion_error(
			std::string                 convert_desc,
			std::string                 detail = "",
			const std::source_location& loc    = std::source_location::current()
		) :
			Detailed_error("Decode error", std::move(detail), loc),
			convert_desc(std::move(convert_desc))
		{
		}

		virtual std::string format() const
		{
			return std::format(
				"At {} [Line {}]:\n\t[Brief] Decoding error\n\t[Decode]{}\n\t[Detail] {}",
				error::crop_file_macro(loc.file_name()),
				loc.line(),
				convert_desc,
				detail
			);
		}
	};

	// > Load 8bit image from file.
	// `path`: Image file path
	// `request_channel`: Requested channel count; normally and default to 4 channels (RGBA); choose 1 for grayscale
	No_discard Stbi_raw_data<uint8_t> load_8bit(const std::string& path, int request_channel = 4);

	// > Load 8bit image from image file data.
	// `file_data`: Image file data
	// `request_channel`: Requested channel count; normally and default to 4 channels (RGBA); choose 1 for grayscale
	No_discard Stbi_raw_data<uint8_t> load_8bit(std::span<const uint8_t> file_data, int request_channel = 4);

	// > Load 16bit image from file.
	// `path`: Image file path
	// `request_channel`: Requested channel count; normally and default to 4 channels (RGBA); choose 1 for grayscale
	No_discard Stbi_raw_data<uint16_t> load_16bit(const std::string& path, int request_channel = 4);

	// > Load 16bit image from image file data.
	// `file_data`: Image file data
	// `request_channel`: Requested channel count; normally and default to 4 channels (RGBA); choose 1 for grayscale
	No_discard Stbi_raw_data<uint16_t> load_16bit(std::span<const uint8_t> file_data, int request_channel = 4);

	// > Load hdr image from file.
	// `path`: Image file path
	No_discard Stbi_raw_data<float> load_hdri(const std::string& path);

	// > Load hdr image from image file data.
	// `file_data`: Image file data
	No_discard Stbi_raw_data<float> load_hdri(std::span<const uint8_t> file_data);
}