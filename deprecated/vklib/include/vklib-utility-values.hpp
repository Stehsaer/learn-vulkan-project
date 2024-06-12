#pragma once
#include "vklib-common.h"

namespace vklib::utility_values
{
	// One-to-one component mapping in creating image view
	extern const VkComponentMapping default_component_mapping;

	// Depth Stencil State with depth test turned on
	extern const VkPipelineDepthStencilStateCreateInfo default_depth_test_enabled;

	// Color Blend State with alpha blending turned on
	extern const VkPipelineColorBlendAttachmentState default_color_blend_state;

	extern const VkPipelineMultisampleStateCreateInfo multisample_state_disabled;

	inline constexpr VkImageSubresourceRange default_subresource_range(VkImageAspectFlags aspect)
	{
		return VkImageSubresourceRange{
			.aspectMask     = aspect,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		};
	}
}