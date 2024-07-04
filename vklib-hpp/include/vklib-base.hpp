#pragma once

#include "vklib-common.hpp"
#include <iostream>
#include <utility>

//* - Mono Resource:
//		Stand-alone resource, no external dependent. Constructed by constructor. eg. Instance, Physical_device
//* - Child Resource:
//		Dependent on external objects to correctly construct and deconstruct. Constructed by constructor. eg. Device

namespace VKLIB_HPP_NAMESPACE
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

	inline std::string crop_file_macro(const std::string& str)
	{
		int len = str.length();
		while (--len >= 0)
			if (str[len] == '/' || str[len] == '\\') return str.substr(len + 1);
		return str;
	}

	struct Exception
	{
		std::string          msg;
		std::source_location loc;

		Exception(std::string _msg, const std::source_location& _loc = std::source_location::current()) :
			msg(std::move(_msg)),
			loc(_loc)
		{
		}
	};

	template <typename T>
	class Mono_resource
	{
	  protected:

		std::shared_ptr<T> data;

		Mono_resource(const T& res) :
			data(std::make_shared<T>(res))
		{
		}

		bool is_unique() const { return data.get() != nullptr && data.use_count() == 1; }

	  public:

		Mono_resource() :
			data(nullptr)
		{
		}

		virtual void clean()     = 0;
		virtual ~Mono_resource() = default;

		Mono_resource(const Mono_resource&) = default;
		Mono_resource(Mono_resource&&)      = default;

		Mono_resource& operator=(const Mono_resource& other)
		{
			clean();
			data = other.data;

			return *this;
		}

		Mono_resource& operator=(Mono_resource&& other) noexcept
		{
			clean();
			data = std::move(other.data);

			return *this;
		}

		void explicit_destroy()
		{
			if (!is_unique()) return;
			clean();
			data = nullptr;
		}

		bool is_valid() const { return data.get() != nullptr; }

		T*       operator->() { return data.get(); }
		const T* operator->() const { return data.get(); }

		operator T() const { return *data; }

		template <typename Dst_T>
			requires std::convertible_to<T, Dst_T>
		Dst_T to() const
		{
			return (Dst_T)(T)(*data);
		}

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
			for (auto i : Range(Size)) ret[i] = arr[i];

			return ret;
		}

		template <typename Derived>
			requires(std::is_convertible_v<Derived, T>)
		inline static std::vector<T> to_vector(const std::vector<Derived>& vec)
		{
			std::vector<T> ret(vec.size());
			for (auto idx : Range(vec.size())) ret[idx] = vec[idx];
			return ret;
		}

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

		std::shared_ptr<Container> data;

		Child_resource(const T& child, const Parent_T& parent) :
			data(std::make_shared<Container>(child, parent))
		{
		}

		bool is_unique() const { return data.get() != nullptr && data.use_count() == 1; }

	  public:

		virtual void clean()      = 0;
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

		void explicit_destroy()
		{
			if (!is_unique()) return;
			clean();
			data = nullptr;
		}

		T*       operator->() { return &data->child; }
		const T* operator->() const { return &data->child; }

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

		template <typename Dst_T = T>
			requires std::convertible_to<T, Dst_T>
		Dst_T to() const
		{
			return (Dst_T)(T)(data->child);
		}

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
			for (auto i : Range(Size)) ret[i] = arr[i];

			return ret;
		}

		template <typename Derived>
			requires(std::is_convertible_v<Derived, T>)
		inline static std::vector<T> to_vector(const std::vector<Derived>& vec)
		{
			std::vector<T> ret(vec.size());
			for (auto idx : Range(vec.size())) ret[idx] = vec[idx];
			return ret;
		}

		T raw() const { return *this; }
	};

	template <typename T>
	class Const_array_proxy
	{
	  private:

		struct Span
		{
			const T* ptr;
			size_t   size;
		};

		static constexpr size_t span_idx = 0, initializer_idx = 1;

		std::variant<Span, std::initializer_list<const T>> _data;

	  public:

		Const_array_proxy() :
			_data(Span(nullptr, 0))
		{
		}

		template <std::ranges::contiguous_range Ty>
			requires(std::is_same_v<T, std::remove_cvref_t<decltype(*(Ty().data()))>>)
		Const_array_proxy(const Ty& vec) :
			_data(Span(vec.data(), vec.size()))
		{
		}

		Const_array_proxy(std::initializer_list<const T>&& init_list) :
			_data(init_list)
		{
		}

		const T* data() const
		{
			switch (_data.index())
			{
			case span_idx:
				return std::get<span_idx>(_data).ptr;
			case initializer_idx:
				return std::data(std::get<initializer_idx>(_data));
			}

			return nullptr;  // backup, ideally can't reach
		}

		size_t size() const
		{
			switch (_data.index())
			{
			case span_idx:
				return std::get<span_idx>(_data).size;
			case initializer_idx:
				return std::size(std::get<initializer_idx>(_data));
			}

			return 0;  // backup, ideally can't reach
		}
	};
}