#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"
#include "inc-imgui.hpp"
#include "vklib.h"
#include <magic_enum.hpp>

using namespace vklib;

#define CHECK_RESULT(result, msg)                                                                                      \
	if (!(result))                                                                                                     \
		throw std::runtime_error(                                                                                      \
			std::format("@{} (Line {}) -- {}: {}", __FUNCTION__, __LINE__, msg, (int)(result).err())                   \
		);

struct Uniform_object
{
	glm::mat4 model, view, projection;
};

struct Vertex_object
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;

	static std::array<VkVertexInputAttributeDescription, 3> get_vertex_attributes(uint32_t binding);
};

struct App_environment
{
	Window                       window;
	Instance                     instance;
	Physical_device              physical_device;
	Device                       device;
	Surface                      surface;
	Vma_allocator                allocator;
	Command_pool                 command_pool;
	std::optional<Debug_utility> debug_utility;

	uint32_t g_queue_family, p_queue_family;
	VkQueue  g_queue, p_queue;

	void init(int init_width, int init_height, const char* title_name);
};

struct App_swapchain
{
	Swapchain               handle;
	uint32_t                image_count;
	std::vector<Image>      images;
	std::vector<Image_view> image_views;
	VkSurfaceFormatKHR      format;
	VkExtent2D              extent;

	Image      depth_buffer;
	Image_view depth_buffer_view;

	void                      init(const App_environment& env);
	static VkSurfaceFormatKHR select_swapchain_format(const Swapchain_detail& swapchain_detail);
};

struct App_synchronization
{
	Semaphore swapchain_ready_semaphore, render_complete_semaphore;
	Fence     idle_fence;

	void init(const App_environment& env);
};

struct App_pipeline
{
	Pipeline_layout   layout;
	Render_pass       render_pass;
	Graphics_pipeline handle;

	Descriptor_set_layout descriptor_set_layout;

	Descriptor_pool             descriptor_pool;
	std::vector<Descriptor_set> descriptor_sets;

	std::vector<Framebuffer> framebuffers;

	void init(const App_environment& env, const App_swapchain& swapchain, bool recreate = false);
	void create_descriptors(const App_environment& env, const App_swapchain& swapchain);
};

struct App_render
{
	Command_buffer command_buf;

	Buffer              vertex_buffer;
	Buffer              index_buffer;
	std::vector<Buffer> uniform_buffers;

	Image         obj_texture;
	Image_view    texture_view;
	Image_sampler texture_sampler;

	App_pipeline pipeline;

	void init(
		const App_environment& env,
		const App_swapchain&   swapchain,
		const std::string&     model_path,
		const std::string&     img_path
	);

	void read_model(const App_environment& env, const std::string& model_path);
	void read_image(const App_environment& env, const std::string& img_path);
	void create_uniform(const App_environment& env, const App_swapchain& swapchain);
};

struct App_imgui_render
{
	Render_pass              render_pass;
	Descriptor_pool          descriptor_pool;
	std::vector<Framebuffer> framebuffers;

	void init(const App_environment& env, const App_swapchain& swapchain);
	void init_graphics(const App_environment& env, const App_swapchain& swapchain);
	void init_imgui(const App_environment& env, const App_swapchain& swapchain);

	void new_frame();

	void destroy();
};

class Obj_app
{
  private:

	App_environment     environment;
	App_swapchain       swapchain;
	App_synchronization sync;
	App_render          renderer;
	App_imgui_render    imgui_render;

	// View angles (in radians)
	float yaw = 0.0f, pitch = 0.0f;
	float scale = 1.0f;

	void update_descriptors(uint32_t swapchain_index);
	void record_command_buffer(uint32_t swapchain_index);
	void recreate_swapchain();
	void draw();
	void imgui_logic();

  public:

	void init(const std::string& model_path, const std::string& img_path);
	void loop();
};