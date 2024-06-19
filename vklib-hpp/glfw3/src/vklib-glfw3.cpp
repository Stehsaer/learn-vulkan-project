#include "vklib-glfw3.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	bool initialize_glfw()
	{
		return glfwInit();
	}

	void terminate_glfw()
	{
		glfwTerminate();
	}

	GLFW_window::GLFW_window(int width, int height, const std::string& title)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		auto* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

		if (window == nullptr)
		{
			const char* err;
			glfwGetError(&err);

			throw Window_exception(std::format("GLFW3 Window Create Error: {}", err));
		}

		*this = GLFW_window(window);
	}

	void GLFW_window::clean()
	{
		if (is_unique()) glfwDestroyWindow(*this);
	}

	std::tuple<int, int> GLFW_window::get_framebuffer_size() const
	{
		int width, height;
		glfwGetFramebufferSize(*this, &width, &height);

		return {width, height};
	}

	std::vector<const char*> GLFW_window::get_extension_names() const
	{
		uint32_t     extension_count;
		const char** extensions;
		extensions = glfwGetRequiredInstanceExtensions(&extension_count);

		std::vector<const char*> list(extensions, extensions + extension_count);

		return list;
	}

	vk::SurfaceKHR GLFW_window::create_surface(const Instance& instance) const
	{
		VkSurfaceKHR surface;

		const auto result = glfwCreateWindowSurface(instance.to<VkInstance>(), *this, nullptr, &surface);

		if (result != VK_SUCCESS)
		{
			const char* err;
			glfwGetError(&err);

			throw Exception(std::format("GLFW3 Window Create Surface Error: {}", err));
		}

		return surface;
	}
}