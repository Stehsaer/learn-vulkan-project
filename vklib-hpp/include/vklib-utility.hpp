#pragma once

#include "vklib-cmdbuf.hpp"
#include "vklib-common.hpp"
#include "vklib-env.hpp"
#include "vklib-io.hpp"
#include "vklib-storage.hpp"

namespace VKLIB_HPP_NAMESPACE::utility
{
	inline constexpr vk::Viewport flip_viewport(const vk::Viewport& viewport)
	{
		return {
			viewport.x,
			viewport.y + viewport.height,
			viewport.width,
			-viewport.height,
			viewport.minDepth,
			viewport.maxDepth
		};
	}

	template <typename Unit_T>
	concept Duration = requires { typename Unit_T::period; };

	struct Cpu_timer
	{
		std::chrono::steady_clock::time_point start_point;
		std::chrono::steady_clock::time_point end_point;

		void start() { start_point = std::chrono::steady_clock::now(); }
		void end() { end_point = std::chrono::steady_clock::now(); }

		template <Duration Unit_T>
		double duration() const
		{
			auto duration = end_point - start_point;
			return std::chrono::duration_cast<std::chrono::duration<double, typename Unit_T::period>>(duration).count();
		}
	};

	template <class T, size_t... Size>
		requires(sizeof...(Size) > 0)
	std::array<T, (Size + ...)> join_array(const std::array<T, Size>&... arr)
	{
		std::array<T, (Size + ...)> ret;

		size_t offset = 0;

		(
			[&](const auto& arr)
			{
				std::copy(arr.begin(), arr.end(), ret.begin() + offset);
				offset += arr.size();
			}(arr),
			...
		);

		return ret;
	}

	template <class T, class... T_item>
		requires((std::is_same_v<T, std::remove_reference<T_item>> && ...))
	std::vector<T> join_array_dynamic(std::span<T_item>... span)
	{
		size_t total_size = (span.size() + ...);

		std::vector<T> ret;
		ret.reserve(total_size);

		(
			[&](const auto& span)
			{
				for (const auto& item : span) ret.push_back(span);
			}(span),
			...
		);

		return ret;
	}

	namespace glm_type_check
	{
		template <typename T, typename Base, glm::qualifier Q, size_t L>
		consteval bool check_glm_vec_recursive()
		{
			if constexpr (L > 0)
			{
				if (std::is_same_v<glm::vec<L, Base, Q>, T>)
					return true;
				else
					return check_glm_vec_recursive<T, Base, Q, L - 1>();
			}
			else
				return false;
		}

		template <typename T, typename Base, glm::qualifier Q, size_t W, size_t H>
		consteval bool check_glm_mat_vertical()
		{
			if constexpr (H > 0)
			{
				if (std::is_same_v<glm::mat<W, H, Base, Q>, T>)
					return true;
				else
					return check_glm_mat_vertical<T, Base, Q, W, H - 1>();
			}
			else
				return false;
		}

		template <typename T, typename Base, glm::qualifier Q, size_t W>
		consteval bool check_glm_mat_horizonal()
		{
			if constexpr (W > 0)
			{
				if (check_glm_mat_vertical<T, Base, Q, W, 4>())
					return true;
				else
					return check_glm_mat_horizonal<T, Base, Q, W - 1>();
			}
			else
				return false;
		}

		// checks if the given type `T` is made up of type `Base`
		template <typename T, typename Base, glm::qualifier Q = glm::defaultp>
		consteval bool check_glm_type()
		{
			bool is_vec  = check_glm_vec_recursive<T, Base, Q, 4>();
			bool is_mat  = check_glm_mat_horizonal<T, Base, Q, 4>();
			bool is_quat = std::is_same_v<T, glm::qua<Base, Q>>;
			bool is_same = std::is_same_v<T, Base>;

			return is_vec || is_mat || is_quat || is_same;
		}
	}
}