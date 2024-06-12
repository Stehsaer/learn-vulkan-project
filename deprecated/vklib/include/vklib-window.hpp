#pragma once

#include "vklib-base.hpp"

namespace vklib
{

	bool                     initialize_glfw();
	void                     terminate_glfw();
	std::vector<const char*> glfw_extension_names();

	class Window : public Base_copyable<GLFWwindow*>
	{
	  public:

		using Base_copyable<GLFWwindow*>::Base_copyable;

		static Result<Window, std::string> create(
			int                width,
			int                height,
			const std::string& title,
			GLFWmonitor*       monitor             = nullptr,
			bool               resizable           = false,
			bool               request_10bit_depth = false
		);

		[[nodiscard]] bool                 should_close() const;
		[[nodiscard]] std::tuple<int, int> get_framebuffer_size() const;
		void                               swap_buffers() const;

		void clean() override;
		~Window() override { clean(); }
	};
}  // namespace vklib