#pragma once

#include "app-common.hpp"

struct App_environment
{
	SDL2_window   window;
	Instance      instance;
	Debug_utility debug_utility;
	Surface       surface;

	Physical_device physical_device;
	uint32_t        g_family_idx, p_family_idx, c_family_idx;
	uint32_t        g_family_count;
	Queue           g_queue, p_queue, t_queue, c_queue;
	Device          device;

	Command_pool                command_pool;
	std::vector<Command_buffer> command_buffer;

	Vma_allocator allocator;

	static Physical_device helper_select_physical_device(const std::vector<Physical_device>& device_list);

	void create_window();
	void create_instance_debug_utility(bool disable_validation = false);
	void find_queue_families();
	void create_logic_device();

	void create();
};

struct App_swapchain
{
	Swapchain            swapchain;
	vk::SurfaceFormatKHR surface_format;
	uint32_t             image_count;
	vk::Extent2D         extent;

	std::vector<vk::Image>  image_handles;
	std::vector<Image_view> image_views;

	void create_swapchain(const App_environment& env);
	void create_images(const App_environment& env);

	void create(App_environment& env);
};
