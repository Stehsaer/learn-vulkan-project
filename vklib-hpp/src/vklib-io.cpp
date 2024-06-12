#include "vklib-io.hpp"
#include <filesystem>
#include <fstream>

namespace VKLIB_HPP_NAMESPACE::io
{
	std::vector<uint8_t> read(const std::string& path)
	{
		// check file existence
		if (!std::filesystem::exists(path)) throw File_exception(path, "File doesn't exists");

		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			throw File_exception(path, "Can't open file for reading");
		}

		// get file size
		file.seekg(0, std::ios::end);
		const size_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(size);
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		{
			throw File_exception(path, "Failed to read file");
		}

		return buffer;
	}

	std::string read_string(const std::string& path)
	{
		const std::ifstream file(path);
		if (!file)
		{
			throw File_exception(path, "Can't open file for reading");
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
			throw File_exception(path, "Can't open file for writing");
		}

		if (!file.write(reinterpret_cast<const char*>(data.data()), data.size()))
		{
			throw File_exception(path, "Failed to write file");
		}
	}
}