#pragma once

#include "vklib-base.hpp"
#include "vklib-common.hpp"
#include "vklib-utility.hpp"

namespace VKLIB_HPP_NAMESPACE::io
{
	class File_exception : public Exception
	{
	  public:

		std::string filename;

		File_exception(
			const std::string&          msg,
			const std::string&          filename,
			const std::source_location& loc = std::source_location::current()
		) :
			Exception(std::format("(File Exception) {}", msg), std::format("File: {}", filename), loc),
			filename(filename)
		{
		}
	};

	[[nodiscard]] std::vector<uint8_t> read(const std::string& path);
	[[nodiscard]] std::string          read_string(const std::string& path);

	void write(const std::string& path, std::span<const uint8_t> data);

	namespace images
	{
		class Image_exception : public Exception
		{
		  public:

			std::string filename, reason;

			Image_exception(
				const std::string&          msg,
				const std::string&          filename,
				const std::string&          reason,
				const std::source_location& loc = std::source_location::current()
			) :
				Exception(std::format("(Image Exception) {}", msg), std::format("{} (File: {})", reason, filename), loc),
				filename(filename),
				reason(reason)
			{
			}
		};

		template <typename Data_T>
		struct Stbi_image_data
		{
			std::vector<Data_T> raw_data;
			int                 width, height;
			int                 original_channels, channels;

			Data_T*       data() { return raw_data.data(); }
			const Data_T* data() const { return raw_data.data(); }
		};

		[[nodiscard]] Stbi_image_data<uint8_t> read_8bit(const std::string& path, int request_channel = 4);
		[[nodiscard]] Stbi_image_data<uint8_t> read_8bit(std::span<const uint8_t> file_data, int request_channel = 4);

		[[nodiscard]] Stbi_image_data<uint16_t> read_16bit(const std::string& path, int request_channel = 4);
		[[nodiscard]] Stbi_image_data<uint16_t> read_16bit(std::span<const uint8_t> file_data, int request_channel = 4);

		[[nodiscard]] Stbi_image_data<float> read_hdri(const std::string& path);
		[[nodiscard]] Stbi_image_data<float> read_hdri(std::span<const uint8_t> file_data);

		struct Stbi_image_utility
		{
		  private:

			Buffer load_hdri_internal(
				const Vma_allocator&                      allocator,
				const Command_buffer&                     command_buffer,
				const io::images::Stbi_image_data<float>& image_data
			);

		  public:

			int   width, height, mipmap_level;
			Image image;

			// Load 8 bit image async;
			// This function doesn't submit to queue, need to externally submit command buffers;
			// Returns staging buffer, clean the staging buffer only after the submit is completed;
			Buffer load_8bit(
				const Physical_device& physical_device,
				const Vma_allocator&   allocator,
				const Command_buffer&  command_buffer,
				const std::string&     path,
				bool                   generate_mipmap,
				bool                   enable_alpha
			);

			// Load 16 bit image async;
			// This function doesn't submit to queue, need to externally submit command buffers;
			// Returns staging buffer, clean the staging buffer only after the submit is completed;
			Buffer load_16bit(
				const Physical_device& physical_device,
				const Vma_allocator&   allocator,
				const Command_buffer&  command_buffer,
				const std::string&     path,
				bool                   generate_mipmap,
				bool                   enable_alpha
			);

			// Load HDRi async;
			// This function doesn't submit to queue, need to externally submit command buffers;
			// Returns staging buffer, clean the staging buffer only after the submit is completed;

			Buffer load_hdri(
				const Vma_allocator&  allocator,
				const Command_buffer& command_buffer,
				const std::string&    path
			);

			Buffer load_hdri(
				const Vma_allocator&     allocator,
				const Command_buffer&    command_buffer,
				std::span<const uint8_t> data
			);
		};
	}

	namespace mesh
	{
		struct Mesh_vertex
		{
			glm::vec3 position, normal, tangent;
			glm::vec2 uv;
		};
	};

	namespace mesh::wavefront
	{
		class Mesh_exception : public Exception
		{
		  public:

			Mesh_exception(const std::string& msg, const std::source_location& loc = std::source_location::current()) :
				Exception(msg, "", loc)
			{
			}
		};

		struct Mesh_data
		{
			std::vector<Mesh_vertex> vertices;
		};

		Mesh_data load_wavefront_obj_mesh(const std::string& file_data);
	}

}