#include "vklib-window.hpp"

// Environment Related
namespace vklib
{
	static bool glfw_initialized = false;

	bool initialize_glfw()
	{
		if (glfw_initialized) return true;

		auto glfw_init_success = glfwInit();
		if (glfw_init_success != GLFW_TRUE) return false;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		glfw_initialized = true;

		return true;
	}

	void terminate_glfw()
	{
		if (glfw_initialized) glfwTerminate();
	}

	std::vector<const char*> glfw_extension_names()
	{
		uint32_t     extension_count;
		const char** extensions;

		extensions = glfwGetRequiredInstanceExtensions(&extension_count);

		std::vector<const char*> list(extensions, extensions + extension_count);

		return list;
	}

}  // namespace vklib

// Class Window
namespace vklib
{
	Result<Window, std::string> Window::create(
		int                width,
		int                height,
		const std::string& title,
		GLFWmonitor*       monitor,
		bool               resizable,
		bool               request_10bit_depth
	)
	{
		if (request_10bit_depth)
		{
			glfwWindowHint(GL_RED_BITS, 10);
			glfwWindowHint(GL_GREEN_BITS, 10);
			glfwWindowHint(GL_BLUE_BITS, 10);
			glfwWindowHint(GL_ALPHA_BITS, 2);
		}

		glfwWindowHint(GLFW_RESIZABLE, resizable);
		GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
		if (window == nullptr)
		{
			const char* err_info;
			glfwGetError(&err_info);
			return std::string(err_info);
		}

		return Window(window);
	}

	bool Window::should_close() const
	{
		return glfwWindowShouldClose(*content);
	}

	void Window::swap_buffers() const
	{
		glfwSwapBuffers(*content);
	}

	std::tuple<int, int> Window::get_framebuffer_size() const
	{
		int w, h;
		glfwGetFramebufferSize(*content, &w, &h);

		return {w, h};
	}

	void Window::clean()
	{
		if (should_delete()) glfwDestroyWindow(*content);
	}
}  // namespace vklib