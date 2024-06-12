#pragma once

#include "vklib-env.hpp"
#include <GLFW/glfw3.h>

namespace VKLIB_HPP_NAMESPACE
{
	bool initialize_glfw();
	void terminate_glfw();

	class GLFW_window : public Mono_resource<GLFWwindow*>, public Window_base
	{
		using Mono_resource<GLFWwindow*>::Mono_resource;

	  public:

		GLFW_window(int width, int height, const std::string& title);

		void clean() override;
		~GLFW_window() override { clean(); }

		std::tuple<int, int>     get_framebuffer_size() const override;
		std::vector<const char*> get_extension_names() const override;
		vk::SurfaceKHR           create_surface(const Instance& instance) const override;
	};
}