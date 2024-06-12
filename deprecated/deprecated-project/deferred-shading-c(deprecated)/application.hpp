#pragma once
#include "check-err.h"
#include "vklib.h"

struct Vertex_data
{
	glm::vec3 pos, normal, uv;

	static consteval std::array<VkVertexInputAttributeDescription, 3> get_vertex_input_attribute(const uint32_t binding)
	{
		// location = 0
		VkVertexInputAttributeDescription pos_description{
			.location = 0,
			.binding  = binding,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = offsetof(Vertex_data, pos)
		};

		// location = 1
		VkVertexInputAttributeDescription normal_description{
			.location = 1,
			.binding  = binding,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = offsetof(Vertex_data, normal)
		};

		// location = 2
		VkVertexInputAttributeDescription uv_description{
			.location = 2,
			.binding  = binding,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = offsetof(Vertex_data, uv)
		};

		return std::to_array({pos_description, normal_description, uv_description});
	}
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

	bool t_queue_available;

	uint32_t g_queue_family, p_queue_family;
	VkQueue  g_queue, p_queue, t_queue;

	void create();
};

struct Render_object_load_status
{
	bool load_albedo = false, load_pbr = false, load_model = false, simplify_model = false;
};

namespace img_loader
{
	enum class Load_result
	{
		Success = 0,
		Read_failed,
		Vk_failed
	};

	Result<Image, Load_result> load_image_8bit(const App_environment& env, const std::string& path, uint32_t mip_level);
}

struct Render_object_data
{
	Image  albedo, pbr;
	Buffer vertex_buffer, index_buffer;

	const std::shared_ptr<Render_object_load_status> load_async(
		const std::string& model_path,
		const std::string& albedo_path,
		const std::string& pbr_path,
		uint32_t           mip_levels
	);
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

	void                      create(const App_environment& env);
	static VkSurfaceFormatKHR select_swapchain_format(const Swapchain_detail& swapchain_detail);
};

struct Deferred_render_target
{
	std::vector<Image>      position, normal, albedo, pbr;
	std::vector<Image_view> position_view, normal_view, albedo_view, pbr_view;
	Image_sampler           deferred_sampler;

	Image      depth_buffer;
	Image_view depth_view;

	Render_pass renderpass;

	std::vector<Framebuffer> framebuffer;

	void create_images(const App_environment& env, const App_swapchain& swapchain);
	void create_renderpass(const App_environment& env);
	void create_framebuffers(const App_environment& env, const App_swapchain& swapchain);

	void create(const App_environment& env, const App_swapchain& swapchain);
};

struct Deferred_pipeline
{
	struct Camera_uniform
	{
		glm::mat4 view_projection_matrix;
	};

	// Model matrix,
	struct Model
	{
		glm::mat4 model;
	};

	Descriptor_set_layout descriptor_set_layout;
	Pipeline_layout       pipeline_layout;

	Descriptor_pool             descriptor_pool;
	std::vector<Descriptor_set> descriptor_set_list;

	std::vector<Buffer> uniform_buffer;

	Graphics_pipeline handle;

	void create_layouts(const App_environment& env, const App_swapchain& swapchain);
	void create_descriptors(const App_environment& env, const App_swapchain& swapchain);
	void create_pipeline(
		const App_environment&        env,
		const App_swapchain&          swapchain,
		const Deferred_render_target& render_target
	);
	void create_uniform_buffer(const App_environment& env, const App_swapchain& swapchain);

	void create(
		const App_environment&        env,
		const App_swapchain&          swapchain,
		const Deferred_render_target& render_target
	);

	void recreate(
		const App_environment&        env,
		const App_swapchain&          swapchain,
		const Deferred_render_target& render_target
	);
};

struct Shadow_pipeline
{
};

struct Shadow_render_target
{
	std::vector<Image>      shadowmap;
	std::vector<Image_view> shadowmap_view;
	uint32_t                current_shadowmap_res = 0;

	Render_pass              renderpass;
	std::vector<Framebuffer> framebuffers;

	void create_shadow_map(const App_environment& env, const App_swapchain& swapchain, uint32_t shadow_map_res);
};

struct Composite_render_target
{
	Render_pass              renderpass;
	std::vector<Framebuffer> framebuffers;

	void create(const App_environment& env, const App_swapchain& swapchain);
};

struct Composite_pipeline
{
	struct Trans_mat
	{
		glm::mat4 view_projection_mat;
		glm::mat4 shadow_mat;
	};

	Descriptor_set_layout descriptor_set_layout;
	Pipeline_layout       pipeline_layout;

	Descriptor_pool             descriptor_pool;
	std::vector<Descriptor_set> descriptor_set_list;

	Graphics_pipeline handle;

	void create_layouts(const App_environment& env, const App_swapchain& swapchain);
	void create_descriptors(const App_environment& env, const App_swapchain& swapchain);
	void create_pipeline(
		const App_environment&         env,
		const App_swapchain&           swapchain,
		const Composite_render_target& render_target
	);

	void create(
		const App_environment&         env,
		const App_swapchain&           swapchain,
		const Composite_render_target& render_target
	);

	void recreate(
		const App_environment&         env,
		const App_swapchain&           swapchain,
		const Composite_render_target& render_target
	);
};

struct Render_core
{
	/* Deferered */

	Deferred_render_target deferred_rt;
	Deferred_pipeline      deferred_pipeline;

	/* Shadow */

	Shadow_pipeline      shadow_pipeline;
	Shadow_render_target shadow_rt;

	/* Composite */

	Composite_pipeline      composite_pipeline;
	Composite_render_target composite_rt;

	void init(
		const App_environment& env,
		const App_swapchain&   swapchain,
		uint32_t               shadow_map_res,
		bool                   recreate = false
	);

	void recreate(const App_environment& env, const App_swapchain& swapchain) { deferred_rt.create(env, swapchain); }
};

struct Application
{
	App_environment env;
	App_swapchain   swapchain;
	Render_core     render_core;

	void init();
};
