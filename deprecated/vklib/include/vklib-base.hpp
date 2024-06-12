#pragma once

#include "vklib-common.h"
#include "vklib-tools.hpp"

#define VERBOSE_DELETE false
#define VERBOSE_CREATE false

namespace vklib
{
	// Base class for copyable object
	template <typename Instance_T>
	class Base_copyable
	{
	  protected:

		std::shared_ptr<Instance_T> content         = nullptr;
		bool                        manage_resource = true;

		Base_copyable(Instance_T val, bool manage_resource = true) :
			content(std::make_shared<Instance_T>(val)),
			manage_resource(manage_resource)
		{

#if VERBOSE_CREATE
			// Print object name and pointer when created
			if constexpr (std::is_convertible_v<Instance_T, void*>)
				tools::print("\033[1;96mCreated \033[0;4;92m{}\033[0m: {}", typeid(Instance_T).name(), (void*)val);
			else  // Can't print pointer
				tools::print("\033[1;96mCreated \033[0;4;92m{}\033[0m", typeid(Instance_T).name());
#endif
		}

		[[nodiscard]] bool should_delete() const
		{

#if VERBOSE_DELETE
			// Print object name and pointer when deleted
			if (content.use_count() == 1 && content.get() != nullptr)
			{
				if constexpr (std::is_convertible_v<Instance_T, void*>)
				{
					tools::print(
						"\033[1;93mDeleted \033[0;4;92m{}\033[0m: {}",
						typeid(Instance_T).name(),
						(void*)*content
					);
				}
				else  // Can't print pointer
				{
					tools::print("\033[1;93mDeleted \033[0;4;92m{}\033[0m", typeid(Instance_T).name());
				}
			}
#endif
			return manage_resource && content.use_count() == 1 && content.get() != nullptr;
		}

		using Type = Instance_T;

		virtual void clean() = 0;

	  public:

		Base_copyable(std::nullptr_t) {}

		Base_copyable(const Base_copyable&) = default;
		Base_copyable(Base_copyable&&)      = default;
		Base_copyable& operator=(const Base_copyable& src)
		{
			clean();
			content = src.content;

			return *this;
		}
		Base_copyable& operator=(Base_copyable&& src) noexcept
		{
			clean();
			content = src.content;

			return *this;
		}
		Base_copyable()          = default;
		virtual ~Base_copyable() = default;

		// Please be cautious when using this function
		void destroy()
		{
			clean();
			content = nullptr;
		}

		Instance_T operator*() const
		{
			if (content) [[likely]]
				return *content;
			return Instance_T();
		}

		Instance_T* operator->() const
		{
			if (content) [[likely]]
				return content.get();
			return nullptr;
		}

		bool is_empty() const { return content.get() == nullptr; }
	};

	template <typename Instance_T, typename Parent_T>
	class Base_copyable_parented : public Base_copyable<Instance_T>
	{
	  protected:

		Parent_T parent = Parent_T();

		Base_copyable_parented(Instance_T val, const Parent_T& parent, bool manage_resource = true) :
			Base_copyable<Instance_T>(val, manage_resource),
			parent(parent)
		{
#if VERBOSE_CREATE
			tools::print("-- \033[36mParent \033[32m{}\033[0m: {}", typeid(Parent_T).name(), (void*)*parent);
#endif
		}

		using Type = Instance_T;

	  public:

		Base_copyable_parented(std::nullptr_t) :
			Base_copyable<Instance_T>(nullptr),
			parent(nullptr)
		{
		}

		Base_copyable_parented(const Base_copyable_parented&)            = default;
		Base_copyable_parented(Base_copyable_parented&&)                 = default;
		Base_copyable_parented& operator=(const Base_copyable_parented&) = default;
		Base_copyable_parented& operator=(Base_copyable_parented&&)      = default;
		Base_copyable_parented()                                         = default;
		virtual ~Base_copyable_parented()                                = default;

		using Base_copyable<Instance_T>::operator*;
		using Base_copyable<Instance_T>::should_delete;
		using Base_copyable<Instance_T>::clean;
		using Base_copyable<Instance_T>::content;

		Parent_T&       get_parent() { return parent; }
		const Parent_T& get_parent() const { return parent; }
	};
}  // namespace vklib