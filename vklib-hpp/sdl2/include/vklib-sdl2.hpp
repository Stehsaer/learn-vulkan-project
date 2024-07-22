#pragma once

#include "vklib-env.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace VKLIB_HPP_NAMESPACE
{
	bool initialize_sdl();
	void terminate_sdl();

	class SDL2_window : public Mono_resource<SDL_Window*>, public Window_base
	{
		using Mono_resource<SDL_Window*>::Mono_resource;

	  public:

		SDL2_window(int width, int height, const std::string& title, SDL_WindowFlags window_flags = {});

		void clean() override;
		~SDL2_window() override { clean(); }

		std::tuple<int, int>     get_framebuffer_size() const override;
		std::vector<const char*> get_extension_names() const override;
		vk::SurfaceKHR           create_surface(const Instance& instance) const override;
	};
}