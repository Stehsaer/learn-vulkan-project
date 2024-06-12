#pragma once
#include "vklib.h"

using namespace vklib;

template <typename Product_T, typename Result_T>
void check(const Result<Product_T, Result_T>& result, const char* msg, const char* file, int line)
{
	if (result) return;

	if constexpr (std::is_same_v<Result_T, VkResult>)
	{
		tools::print("Error at {}:{} -> {} (Result={})", file, line, msg, (int)result.err());
	}
	else
	{
		tools::print("Error at {}:{} -> {}", file, line, msg);
	}

	throw std::runtime_error(std::format("Error at {}:{} -> {}", file, line, msg));
}

inline void check(VkResult result, const char* msg, const char* file, int line)
{
	if (result == VK_SUCCESS) return;

	tools::print("Error at {}:{} -> {} (Result={})", file, line, msg, (int)result);
	throw std::runtime_error(std::format("Error at {}:{} -> {}", file, line, msg));
}

inline void check(bool result, const char* msg, const char* file, int line)
{
	if (result) return;

	tools::print("Error at {}:{} -> {}", file, line, msg);
	throw std::runtime_error(std::format("Error at {}:{} -> {}", file, line, msg));
}

inline consteval const char* crop_file_macro(const char* str)
{
	int len = 0;
	while (str[len]) len++;
	while (--len >= 0)
		if (str[len] == '/' || str[len] == '\\') return str + len + 1;
	return str;
}

#define CHECK_RESULT(expr, msg) check((expr), (msg), crop_file_macro(__FILE__), __LINE__)
