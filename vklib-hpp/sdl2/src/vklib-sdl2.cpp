#include "vklib-sdl2.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	bool initialize_sdl()
	{
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
		return !SDL_Init(SDL_INIT_VIDEO);
	}

	void terminate_sdl()
	{
		SDL_Quit();
	}

	SDL2_window::SDL2_window(int width, int height, const std::string& title, SDL_WindowFlags window_flags)
	{
		auto combined_window_flags = SDL_WINDOW_VULKAN | window_flags;

		auto* new_window = SDL_CreateWindow(
			title.c_str(),
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width,
			height,
			combined_window_flags
		);

		if (new_window == nullptr)
		{
			const char* err = SDL_GetError();
			throw Window_exception(err);
		}

		*this = SDL2_window(new_window);
	}

	void SDL2_window::clean()
	{
		if (is_unique()) SDL_DestroyWindow(*this);
	}

	std::tuple<int, int> SDL2_window::get_framebuffer_size() const
	{
		int width, height;
		SDL_Vulkan_GetDrawableSize(*this, &width, &height);

		return {width, height};
	}

	std::vector<const char*> SDL2_window::get_extension_names() const
	{
		uint32_t extension_count;
		SDL_Vulkan_GetInstanceExtensions(*this, &extension_count, nullptr);

		std::vector<const char*> list(extension_count);
		SDL_Vulkan_GetInstanceExtensions(*this, &extension_count, list.data());

		return list;
	}

	vk::SurfaceKHR SDL2_window::create_surface(const Instance& instance) const
	{
		VkSurfaceKHR surface;
		auto         result = SDL_Vulkan_CreateSurface(
            *this,
            instance.to<VkInstance>(),
            &surface
        );  // glfwCreateWindowSurface((VkInstance)(vk::Instance)instance, window, nullptr, &surface);

		if (result != SDL_TRUE)
		{
			const char* err_info = SDL_GetError();

			throw Exception(err_info);
		}

		return surface;
	}
}