#pragma once

#include "vklib-common.h"
#include <cassert>
#include <iostream>
#include <map>

namespace vklib
{
	template <typename T>
		requires(std::is_integral_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>)
	class Range
	{
		std::remove_cvref_t<T> start_val, end_val;

		class Iterator
		{
			T val;

			friend Range;

			Iterator(T val) noexcept :
				val(val)
			{
			}

		  public:

			T         operator*() const noexcept { return val; }
			Iterator& operator++() noexcept
			{
				val++;
				return *this;
			}
			bool operator==(const Iterator& _val) const noexcept { return val == _val.val; }
			bool operator!=(const Iterator& _val) const noexcept { return val != _val.val; }
		};

	  public:

		template <typename T1>
			requires(std::is_convertible_v<std::remove_cvref_t<T1>, std::remove_cvref_t<T>>)
		Range(T begin, T1 end) :
			start_val(begin),
			end_val(end)
		{
			assert((T)end >= begin);
		}

		Range(T end) :
			start_val(0),
			end_val(end)
		{
			assert(end >= (T)0);
		}

		Iterator begin() const noexcept { return Iterator(start_val); }
		Iterator end() const noexcept { return Iterator(end_val); }
	};

	template <typename Product_T, typename Result_T>
	class Result
	{
		std::optional<Product_T> obj;
		std::optional<Result_T>  result;
		bool                     ok_flag;

	  public:

		Result(Product_T&& obj) :
			obj(std::forward<Product_T>(obj)),
			result(std::nullopt),
			ok_flag(true)
		{
		}

		Result(Result_T&& result) :
			obj(std::nullopt),
			result(std::forward<Result_T>(result)),
			ok_flag(false)
		{
		}

		Result()              = delete;
		Result(const Result&) = delete;
		Result(Result&&)      = default;

		operator bool() const { return ok_flag; }

		template <typename T>
		Result&& operator>>(T& target)
		{
			if (obj.has_value()) target = *obj;
			return std::forward<Result>(*this);
		}

		Result_T&& err() { return std::forward<Result_T>(*result); }

		const Result_T& err() const { return *result; }

		Product_T&& val() { return std::forward<Product_T>(*obj); }
	};
}

namespace vklib::tools
{
	template <typename... Args>
	inline void print(std::format_string<Args...> fmt, Args&&... args)
	{
		std::cout << std::format(fmt, std::forward<Args>(args)...) << "\n";
	}

	Result<std::vector<uint8_t>, std::string> read_binary(const std::string& filename);

	inline void flip_viewport(VkViewport& viewport)
	{
		viewport.y      = viewport.height;
		viewport.height = -viewport.height;
	}
}