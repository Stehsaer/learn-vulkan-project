#include "vklib-tools.hpp"
#include <fstream>

namespace vklib::tools
{
	Result<std::vector<uint8_t>, std::string> read_binary(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		if (!file.is_open()) return std::string("Can't open file ") + filename;

		const uint32_t file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(file_size);
		file.read(reinterpret_cast<char*>(buffer.data()), file_size);

		file.close();

		return buffer;
	}

}
