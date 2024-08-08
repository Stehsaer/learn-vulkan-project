#pragma once

#include "vklib-common.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	// Walks through indices.
	// -- designed to work like `range()` in Python language
	template <typename T>
		requires(std::is_integral_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>)
	class Iota
	{
		std::remove_cvref_t<T> start_val, end_val;

		class Iterator
		{
			T val;

			friend Iota;

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
		Iota(T begin, T1 end) :
			start_val(begin),
			end_val(end)
		{
			assert((T)end >= begin);
		}

		Iota(T end) :
			start_val(0),
			end_val(end)
		{
			assert(end >= (T)0);
		}

		Iterator begin() const noexcept { return Iterator(start_val); }
		Iterator end() const noexcept { return Iterator(end_val); }
	};

	template <typename T>
	concept Is_iterable = requires(T obj) {
		obj.begin();
		obj.end();
		obj.begin()++;
		obj.begin() == obj.end();
		obj.end() - obj.begin();
	};

	// Walks through a contiguous container.
	// -- works with `std::span`, `std::array`, `std::vector`
	// -- works with any customized types that satisfies `Is_iterable`
	// -- the iterator returns (index, item)
	// -- designed to be used in: `for(auto [index, item] : Walk(...container...))`
	template <Is_iterable T>
	class Walk
	{
		T& container;

		using Container_iterator = decltype(T().begin());
		using Container_type     = std::remove_reference_t<decltype(*T().begin())>;

	  public:

		//> Walks through a contiguous container.
		// -- `container`: Target container.
		Walk(T& container) noexcept :
			container(container)
		{
		}

		class Iterator
		{
			T&                 container;
			Container_iterator iter;

			friend Walk;

			Iterator(T& container, Container_iterator iter) noexcept :
				container(container),
				iter(iter)
			{
			}

			struct Get_value_struct
			{
				const size_t    index;
				Container_type& item;
			};

		  public:

			void operator++() { iter++; }

			Get_value_struct operator*() const { return {(size_t)(iter - container.begin()), *iter}; }

			auto operator!=(const Iterator& other) const noexcept { return iter != other.iter; }
		};

		Iterator begin() const noexcept { return {container, container.begin()}; }
		Iterator end() const noexcept { return {container, container.end()}; }
	};

	template <typename T>
	class Array_proxy
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

		Array_proxy() :
			_data(Span(nullptr, 0))
		{
		}

		template <std::ranges::contiguous_range Ty>
			requires(std::is_same_v<T, std::remove_cvref_t<decltype(*(Ty().data()))>>)
		Array_proxy(const Ty& vec) :
			_data(Span(vec.data(), vec.size()))
		{
		}

		Array_proxy(std::initializer_list<const T>&& init_list) :
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