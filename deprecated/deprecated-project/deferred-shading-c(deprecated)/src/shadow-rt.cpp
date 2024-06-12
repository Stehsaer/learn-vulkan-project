#include "application.hpp"

void Shadow_render_target::create_shadow_map(
	const App_environment& env,
	const App_swapchain&   swapchain,
	uint32_t               shadow_map_res
)
{
	// Configure
	current_shadowmap_res = shadow_map_res;

	// Cleanup
	shadowmap.clear();
	shadowmap_view.clear();

	// Create
	for (auto i : Range(swapchain.image_count))
	{
		auto result = Image::create(
						  env.allocator,
						  VK_IMAGE_TYPE_2D,
						  {shadow_map_res, shadow_map_res, 1},
						  VK_FORMAT_D32_SFLOAT,
						  VK_IMAGE_TILING_OPTIMAL,
						  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						  VMA_MEMORY_USAGE_GPU_ONLY,
						  VK_SHARING_MODE_EXCLUSIVE
					  )
			>> shadowmap.emplace_back();
		CHECK_RESULT(result, "Create Shadow Map Failed");

		auto view_result = Image_view::create(
							   env.device,
							   shadowmap[i],
							   VK_FORMAT_D32_SFLOAT,
							   utility_values::default_component_mapping,
							   VK_IMAGE_VIEW_TYPE_2D,
							   {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
						   )
			>> shadowmap_view.emplace_back();
		CHECK_RESULT(result, "Create Shadow Map View Failed");
	}
}
