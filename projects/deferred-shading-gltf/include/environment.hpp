#pragma once

#include <vklib-sdl2.hpp>
#include <vklib>

inline constexpr uint32_t csm_count              = 3;
inline constexpr uint32_t bloom_downsample_count = 8;

using namespace VKLIB_HPP_NAMESPACE;

class Environment
{
  public:

	struct
	{
		bool validation_layer_enabled;
		bool debug_marker_enabled;
		bool  anistropy_enabled;
		float max_anistropy = 0.0;
	} features;

	SDL2_window   window;
	Instance      instance;
	Debug_utility debug_utility;
	Debug_marker  debug_marker;
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

		struct
		{
			bool color_depth_10_enabled = false;
			bool hdr_enabled            = false;
		} feature;

		void create_swapchain(const Environment& env);
		void create_images(const Environment& env);

		void create(Environment& env);
	};

	Env_swapchain swapchain;

	void create();

	template <typename... T>
	void log_msg(const std::format_string<std::remove_reference_t<T>...> fmt, T&&... args) const
	{
		auto now = std::chrono::system_clock::now();
		std::cout << std::format("[{:%H:%M:%S} - LOG] ", now) << std::format(fmt, std::move(args)...) << "\n";
	}

	template <typename... T>
	void log_err(const std::format_string<T...> fmt, T&&... args) const
	{
		auto now = std::chrono::system_clock::now();
		std::cerr << std::format("\033[1;[{:%H:%M:%S} - ERR] ", now) << std::format(fmt, args...) << "\033[0m" << std::endl;
	}

  private:

	static Physical_device helper_select_physical_device(const std::vector<Physical_device>& device_list);

	void create_window();
	void create_instance_debug_utility(bool disable_validation = false);
	void find_queue_families();
	void create_logic_device();
};
