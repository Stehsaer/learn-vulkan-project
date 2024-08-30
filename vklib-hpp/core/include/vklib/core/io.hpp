// vklib/core/io.hpp
// ================
// [Author] Hsin-chieh Liu (Stehsaer)
// ================
// [Description]
// - Defines IO_exception class
// - Provides very basic file reading & writing functions

#pragma once

#include "vklib/core/base.hpp"
#include "vklib/core/common.hpp"
#include "vklib/core/utility.hpp"


namespace VKLIB_HPP_NAMESPACE::io
{
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
		class Mesh_exception : public error::Detailed_error
		{
		  public:

			Mesh_exception(const std::string& msg, const std::source_location& loc = std::source_location::current()) :
				Detailed_error(msg, "", loc)
			{
			}
		};
	}

}