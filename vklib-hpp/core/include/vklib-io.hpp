// vklib-io.hpp
// ================
// [Author] Hsin-chieh Liu (Stehsaer)
// ================
// [Description]
// - Defines IO_exception class
// - Provides very basic file reading & writing functions

#pragma once

#include "vklib-base.hpp"
#include "vklib-common.hpp"
#include "vklib-utility.hpp"

namespace VKLIB_HPP_NAMESPACE::io
{
	// IO Exception Class
	class IO_exception : public Exception
	{
	  public:

		IO_exception(
			const std::string&          msg,
			const std::string&          detail,
			const std::string&          filename,
			const std::source_location& loc = std::source_location::current()
		) :
			Exception(std::format("(IO Exception) {}", msg), std::format("{} (File: {})", detail, filename), loc)
		{
		}

		IO_exception(
			const std::string&          msg,
			const std::string&          detail,
			const std::source_location& loc = std::source_location::current()
		) :
			Exception(std::format("(IO Exception) {}", msg), std::format("{}", detail), loc)
		{
		}
	};

	// > Reads binary data from a file.
	// Returns `std::vector<uint8_t>`
	No_discard std::vector<uint8_t> read(const std::string& path);

	// > Reads text data from a file.
	// Returns `std::string`
	No_discard std::string read_string(const std::string& path);

	// > Writes binary data to a file
	void write(const std::string& path, std::span<const uint8_t> data);

	namespace mesh
	{
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
	}

}