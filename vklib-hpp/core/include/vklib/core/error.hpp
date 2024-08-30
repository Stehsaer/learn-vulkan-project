#pragma once

#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

#include "vklib/core/common.hpp"

namespace VKLIB_HPP_NAMESPACE::error
{
	inline constexpr std::string crop_file_macro(const std::string& str)
	{
		int len = str.length();
		while (--len >= 0)
			if (str[len] == '/' || str[len] == '\\') return str.substr(len + 1);
		return str;
	}

	// Class of detailed error messages
	struct Detailed_error : public std::exception
	{
		std::string          msg, detail;
		std::source_location loc;

		Detailed_error(std::string _msg, std::string detail = "", const std::source_location& _loc = std::source_location::current()) :
			msg(std::move(_msg)),
			detail(std::move(detail)),
			loc(_loc)
		{
		}

		virtual std::string format() const
		{
			return std::format(
				"At {} [Line {}]:\n\t[Brief] {}\n\t[Detail] {}",
				crop_file_macro(loc.file_name()),
				loc.line(),
				msg,
				detail
			);
		}

		virtual const char* what() const noexcept override final { return msg.c_str(); }
	};

	// Class of invalid argument error
	struct Invalid_argument : public Detailed_error
	{
		Invalid_argument(std::string detail = "", const std::source_location& loc = std::source_location::current()) :
			Detailed_error("Invalid argument", std::move(detail), loc)
		{
		}

		static void check(bool condition, std::string detail = "", const std::source_location& loc = std::source_location::current())
		{
			if (!condition) throw Invalid_argument(std::move(detail), loc);
		}

		virtual std::string format() const
		{
			return std::format(
				"At {} [Line {}]:\n\t[Brief] Invalid argument\n\t[Function] {}\n\t[Detail] {}",
				crop_file_macro(loc.file_name()),
				loc.line(),
				loc.function_name(),
				detail
			);
		}
	};

#define QUICK_CHECK_ARGUMENT(condition) VKLIB_HPP_NAMESPACE::error::Invalid_argument::check(condition, #condition)

	// Class of not implemented error
	struct Not_implemented : public Detailed_error
	{
		Not_implemented(std::string detail = "", const std::source_location& loc = std::source_location::current()) :
			Detailed_error("Not implemented", std::move(detail), loc)
		{
		}

		virtual std::string format() const
		{
			return std::format(
				"At {} [Line {}]:\n\t[Brief] Not Implemented\n\t[Function] {}\n\t[Detail] {}",
				crop_file_macro(loc.file_name()),
				loc.line(),
				loc.function_name(),
				detail
			);
		}
	};

	// Class of IO error
	struct IO_error : public Detailed_error
	{
		std::string target;

		IO_error(std::string target, std::string detail = "", const std::source_location& loc = std::source_location::current()) :
			Detailed_error("IO error", std::move(detail), loc),
			target(std::move(target))
		{
		}

		virtual std::string format() const
		{
			return std::format(
				"At {} [Line {}]:\n\t[Brief] IO error\n\t[Target] {}\n\t[Detail] {}",
				crop_file_macro(loc.file_name()),
				loc.line(),
				target,
				detail
			);
		}
	};

}