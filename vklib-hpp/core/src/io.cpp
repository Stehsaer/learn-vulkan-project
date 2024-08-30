#include "vklib/core/io.hpp"
#include <filesystem>
#include <fstream>

#define IO_READ_FAIL_MSG "Failed to read file"
#define IO_READ_WRITE_MSG "Failed to write file"

#define FILENAME_TO_STRING(filename) std::format("File: {}", filename)

namespace VKLIB_HPP_NAMESPACE::io
{
	std::vector<uint8_t> read(const std::string& path)
	{
		// check file existence
		if (!std::filesystem::exists(path)) throw error::IO_error(FILENAME_TO_STRING(path), "File doesn't exist");

		std::ifstream file(path, std::ios::binary);
		if (!file) throw error::IO_error(FILENAME_TO_STRING(path), "Can't open file for reading");

		// get file size
		file.seekg(0, std::ios::end);
		const size_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(size);
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		{
			throw error::IO_error(FILENAME_TO_STRING(path), "Failed to read file");
		}

		return buffer;
	}

	std::string read_string(const std::string& path)
	{
		const std::ifstream file(path);
		if (!file) throw error::IO_error(FILENAME_TO_STRING(path), "Can't open file for reading");

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void write(const std::string& path, std::span<const uint8_t> data)
	{
		std::ofstream file(path, std::ios::binary);
		if (!file) throw error::IO_error(FILENAME_TO_STRING(path), "Can't open file for writing");

		if (!file.write(reinterpret_cast<const char*>(data.data()), data.size()))
			throw error::IO_error(FILENAME_TO_STRING(path), "Failed to write file");
	}
}