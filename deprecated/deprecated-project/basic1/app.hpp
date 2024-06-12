#pragma once

#include "magic_enum.hpp"
#include "vklib.h"
#include <algorithm>
#include <iostream>

using namespace vklib;

template <typename Product_T, typename Result_T, typename... Arg_T>
void check_success(const Result<Product_T, Result_T>& condition, std::format_string<Arg_T...> fmt, Arg_T... args)
{
	if (!condition)
	{
		// tools::print(
		// 	"Check failed on Result<{},{}>", typeid(Product_T).name(),
		// typeid(Result_T).name()
		// );
		std::cout << "ERR: ";
		tools::print(fmt, std::forward<Arg_T>(args)...);
		if constexpr (std::is_same_v<Result_T, VkResult>)
		{
			tools::print("VkResult: {}", magic_enum::enum_name<VkResult>(condition.err()));
		}
		else if constexpr (std::is_same_v<Result_T, std::string>)
		{
			tools::print("Error String: {}", condition.err());
		}
		exit(-1);
	}
}

template <typename Cond_T, typename... Arg_T>
	requires(std::is_same_v<bool, Cond_T> || std::is_same_v<VkResult, Cond_T>)
void check_success(Cond_T condition, std::format_string<Arg_T...> fmt, Arg_T... args)
{
	if constexpr (std::is_same_v<bool, Cond_T>)
	{
		if (!condition)
		{
			std::cout << "ERR: ";
			tools::print(fmt, std::forward<Arg_T>(args)...);
			exit(-1);
		}
	}
	else if constexpr (std::is_same_v<VkResult, Cond_T>)
		if (condition != VK_SUCCESS)
		{
			std::cout << "ERR: ";
			tools::print(fmt, std::forward<Arg_T>(args)...);
			tools::print("VkResult: {}", magic_enum::enum_name<VkResult>(condition));
			exit(-1);
		}
}

const int width = 800, height = 600;

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 texcoord;

	static std::array<VkVertexInputAttributeDescription, 2> get_vertex_attributes(uint32_t binding);
};

struct Uniform_object
{
	glm::mat4 model, view, projection;
};

inline const std::vector<Vertex> vertex_list = {
	{{-0.5f, -0.5f, 0.0f},  {0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f},   {1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f},    {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f},   {0.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f},  {1.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f},   {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f},  {0.0f, 1.0f}}
};

inline const std::vector<uint16_t> index_list = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

class Application
{
  public:

	void init();
	void run();

  private:

	/* Basic */

	Window                       window;
	Instance                     instance;
	std::optional<Debug_utility> debug_utils = std::nullopt;

	std::vector<const char*> layers;

	/* Device-Related */

	uint32_t graphics_queue_idx;
	VkQueue  graphics_queue;

	uint32_t presentation_queue_idx;
	VkQueue  presentation_queue;

	Physical_device physical_device;
	Device          main_device;
	Surface         surface;
	Vma_allocator   allocator;

	/* Swapchain-Related */
	VkSurfaceFormatKHR swapchain_image_format;
	VkPresentModeKHR   present_mode;

	VkExtent2D              swap_extent;
	Swapchain               swapchain;
	uint32_t                swapchain_image_count;
	std::vector<Image>      swapchain_images;
	std::vector<Image_view> swapchain_image_views;

	Image      depth_buffer;
	Image_view depth_buffer_view;

	std::vector<Framebuffer> framebuffers;

	/* Pipeline */
	Pipeline_layout   pipeline_layout;
	Render_pass       render_pass;
	Graphics_pipeline graphics_pipeline;

	/* Command */
	Command_pool   command_pool;
	Command_buffer command_buffer;

	/* Synchronization Objects */
	Semaphore render_complete_semaphore, image_available_semaphore;
	Fence     next_frame_fence;

	/* Buffers */
	Buffer              vertex_buffer;
	Buffer              index_buffer;
	std::vector<Buffer> uniform_buffers;

	/* Descriptors */
	Descriptor_pool             descriptor_pool;
	Descriptor_set_layout       descriptor_set_layout;
	std::vector<Descriptor_set> descriptor_sets;

	/* Images */
	Image         texture;
	Image_view    texture_image_view;
	Image_sampler sampler;

	/* Initialization Functions */

	void init_window();
	void init_instance();
	void init_device();
	void init_swapchain();
	void init_pipeline();
	void init_command_buffer();
	void init_signals();
	void init_buffers();
	void init_descriptors();
	void init_texture();
	void init_depth_buffer();

	/* Runtime loop */

	void record_command_buffer(uint32_t index);
	void bind_descriptors(uint32_t index) const;
	void test_recreate_swapchain();
	void main_draw();
};
