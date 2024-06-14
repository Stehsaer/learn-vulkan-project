#pragma once

#include <vklib-sdl2.hpp>
#include <vklib>

inline constexpr uint32_t csm_count              = 3;
inline constexpr uint32_t bloom_downsample_count = 8;

using namespace VKLIB_HPP_NAMESPACE;

class Environment
{
  public:

	SDL2_window   window;
	Instance      instance;
	Debug_utility debug_utility;
	Surface       surface;

	Physical_device physical_device;
	Device          device;

	uint32_t g_family_idx, p_family_idx, c_family_idx;
	uint32_t g_family_count;
	Queue    g_queue, p_queue, t_queue, c_queue;

	Command_pool                command_pool;
	std::vector<Command_buffer> command_buffer;

	Vma_allocator allocator;

	struct Env_swapchain
	{
		Swapchain            swapchain;
		vk::SurfaceFormatKHR surface_format;
		uint32_t             image_count;
		vk::Extent2D         extent;

		std::vector<vk::Image>  image_handles;
		std::vector<Image_view> image_views;

		void create_swapchain(const Environment& env);
		void create_images(const Environment& env);

		void create(Environment& env);
	};

	Env_swapchain swapchain;

	void create();

  private:

	static Physical_device helper_select_physical_device(const std::vector<Physical_device>& device_list);

	void create_window();
	void create_instance_debug_utility(bool disable_validation = false);
	void find_queue_families();
	void create_logic_device();
};
