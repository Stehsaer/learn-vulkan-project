#pragma once

#include "vklib/core/common.hpp"
#include "vklib/core/helper.hpp"
#include <iostream>
#include <utility>

#define No_discard [[nodiscard]]

namespace VKLIB_HPP_NAMESPACE
{
	inline constexpr std::string crop_file_macro(const std::string& str)
	{
		int len = str.length();
		while (--len >= 0)
			if (str[len] == '/' || str[len] == '\\') return str.substr(len + 1);
		return str;
	}

	struct Exception
	{
		std::string          msg, detail;
		std::source_location loc;

		Exception(std::string _msg, std::string detail = "", const std::source_location& _loc = std::source_location::current()) :
			msg(std::move(_msg)),
			detail(std::move(detail)),
			loc(_loc)
		{
		}
	};

	struct Invalid_argument : public Exception
	{
		std::string param_name, expect;

		Invalid_argument(
			const std::string&          msg,
			std::string                 param_name,
			std::string                 expect = "",
			const std::source_location& _loc   = std::source_location::current()
		) :
			Exception(msg, std::format("Parameter \"{}\" Invalid. Expects: {}", param_name, expect), _loc),
			param_name(std::move(param_name)),
			expect(std::move(expect))
		{
		}
	};

	template <typename T>
	class Mono_resource
	{
	  protected:

		std::shared_ptr<T> data = nullptr;

		Mono_resource(const T& res) :
			data(std::make_shared<T>(res))
		{
		}

		bool is_unique() const { return data.get() != nullptr && data.use_count() == 1; }

		virtual void clean() = 0;

	  public:

		Mono_resource() :
			data(nullptr)
		{
		}

		virtual ~Mono_resource() = default;

		Mono_resource(const Mono_resource&) = default;
		Mono_resource(Mono_resource&&)      = default;

		Mono_resource& operator=(const Mono_resource& other)
		{
			if (other.data == data) [[unlikely]]
				return *this;

			clean();
			data = other.data;

			return *this;
		}

		Mono_resource& operator=(Mono_resource&& other) noexcept
		{
			if (other.data == data) [[unlikely]]
				return *this;

			clean();
			data = std::move(other.data);

			return *this;
		}

		// Destroy the object explicitly
		void explicit_destroy()
		{
			if (!is_unique()) return;
			clean();
			data = nullptr;
		}

		// Checks if the object is valid (internal data isn't `nullptr`)
		bool is_valid() const { return data.get() != nullptr; }

		T*       operator->() { return data.get(); }
		const T* operator->() const { return data.get(); }

		operator T() const
		{
			if (data) [[likely]]
				return *data;
			else
				return T();
		}

		operator std::span<const T>() const
		{
			if (data) [[likely]]
				return std::span<T>(data.get(), 1);
			else
				return std::span<T>();
		}

		operator vk::ArrayProxy<const T>() const
		{
			if (data) [[likely]]
				return vk::ArrayProxy<T>(1, data.get());
			else
				return vk::ArrayProxy<T>();
		}

		// Explicitly convert to `Dst_T`
		template <typename Dst_T>
			requires std::convertible_to<T, Dst_T>
		Dst_T to() const
		{
			return (Dst_T)(T)(*data);
		}

		// Convert an array of vklib objects to vulkan objects
		template <size_t Size>
		inline static std::array<T, Size> to_array(T (&&arr)[Size])
		{
			return std::to_array<T>(arr);
		}

		template <typename Ty, size_t Size>
			requires(std::is_base_of_v<Mono_resource<T>, Ty>)
		inline static std::array<T, Size> to_array(const std::array<Ty, Size>& arr)
		{
			std::array<T, Size> ret;
			for (auto i : Iota(Size)) ret[i] = arr[i];

			return ret;
		}

		// Convert a vector of vklib objects to vulkan objects
		template <typename Derived>
			requires(std::is_convertible_v<Derived, T>)
		inline static std::vector<T> to_vector(const std::vector<Derived>& vec)
		{
			std::vector<T> ret(vec.size());
			for (auto idx : Iota(vec.size())) ret[idx] = vec[idx];
			return ret;
		}

		// Returns the underlying raw object
		T raw() const { return *this; }
	};

	template <typename T, typename Parent_T>
	class Child_resource
	{
	  protected:

		struct Container
		{
			T        child;
			Parent_T parent;
		};

		std::shared_ptr<Container> data = nullptr;

		Child_resource(const T& child, const Parent_T& parent) :
			data(std::make_shared<Container>(child, parent))
		{
		}

		bool         is_unique() const { return data.get() != nullptr && data.use_count() == 1; }
		virtual void clean() = 0;

	  public:

		virtual ~Child_resource() = default;

		Child_resource(const Child_resource&) = default;
		Child_resource(Child_resource&&)      = default;

		Child_resource() :
			data(nullptr)
		{
		}

		Child_resource& operator=(const Child_resource& other)
		{
			clean();
			data = other.data;

			return *this;
		}

		Child_resource& operator=(Child_resource&& other) noexcept
		{
			clean();
			data = std::move(other.data);

			return *this;
		}

		// Destroy the object explicitly
		void explicit_destroy()
		{
			if (!is_unique()) return;
			clean();
			data = nullptr;
		}

		T*       operator->() { return &data->child; }
		const T* operator->() const { return &data->child; }

		// Checks if the object is valid (internal data isn't `nullptr`)
		bool is_valid() const { return data.get() != nullptr; }

		Parent_T&       parent() { return data->parent; }
		const Parent_T& parent() const { return data->parent; }

		operator T() const
		{
			if (data) [[likely]]
				return data->child;
			else
				return T();
		}

		operator std::span<const T>() const
		{
			if (data) [[likely]]
				return std::span<T>(&data->child, 1);
			else
				return std::span<T>();
		}

		operator vk::ArrayProxy<const T>() const
		{
			if (data) [[likely]]
				return vk::ArrayProxy<T>(1, &data->child);
			else
				return vk::ArrayProxy<T>();
		}

		// Explicitly convert to `Dst_T`
		template <typename Dst_T = T>
			requires std::convertible_to<T, Dst_T>
		Dst_T to() const
		{
			return (Dst_T)(T)(data->child);
		}

		// Convert an array of vklib objects to vulkan objects
		template <size_t Size>
		inline static std::array<T, Size> to_array(T (&&arr)[Size])
		{
			return std::to_array<T>(arr);
		}

		template <typename Ty, size_t Size>
			requires(std::is_base_of_v<Child_resource<T, Parent_T>, Ty>)
		inline static std::array<T, Size> to_array(const std::array<Ty, Size>& arr)
		{
			std::array<T, Size> ret;
			for (auto i : Iota(Size)) ret[i] = arr[i];

			return ret;
		}

		// Convert a vector of vklib objects to vulkan objects
		template <typename Derived>
			requires(std::is_convertible_v<Derived, T>)
		inline static std::vector<T> to_vector(const std::vector<Derived>& vec)
		{
			std::vector<T> ret(vec.size());
			for (auto idx : Iota(vec.size())) ret[idx] = vec[idx];
			return ret;
		}

		T raw() const { return *this; }
	};
}