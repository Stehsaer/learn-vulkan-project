#include "vklib-io.hpp"
#include <filesystem>
#include <fstream>

#define IO_READ_FAIL_MSG "Failed to read file"
#define IO_READ_WRITE_MSG "Failed to write file"

namespace VKLIB_HPP_NAMESPACE::io
{
	std::vector<uint8_t> read(const std::string& path)
	{
		// check file existence
		if (!std::filesystem::exists(path)) throw IO_exception(IO_READ_FAIL_MSG, "File doesn't exist", path);

		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			throw IO_exception(IO_READ_FAIL_MSG, "Can't open file for reading", path);
		}

		// get file size
		file.seekg(0, std::ios::end);
		const size_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(size);
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		{
			throw IO_exception(IO_READ_FAIL_MSG, "Failed to read file", path);
		}

		return buffer;
	}

	std::string read_string(const std::string& path)
	{
		const std::ifstream file(path);
		if (!file)
		{
			throw IO_exception(IO_READ_FAIL_MSG, "Can't open file for reading", path);
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void write(const std::string& path, std::span<const uint8_t> data)
	{
		std::ofstream file(path, std::ios::binary);
		if (!file)
		{
			throw IO_exception(IO_READ_WRITE_MSG, "Can't open file for writing", path);
		}

		if (!file.write(reinterpret_cast<const char*>(data.data()), data.size()))
		{
			throw IO_exception(IO_READ_WRITE_MSG, "Unknown reason", path);
		}
	}
}