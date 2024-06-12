#include "vklib-utility-values.hpp"

namespace vklib::utility_values
{
	const VkComponentMapping default_component_mapping
		= {VK_COMPONENT_SWIZZLE_IDENTITY,
		   VK_COMPONENT_SWIZZLE_IDENTITY,
		   VK_COMPONENT_SWIZZLE_IDENTITY,
		   VK_COMPONENT_SWIZZLE_IDENTITY};

	const VkPipelineDepthStencilStateCreateInfo default_depth_test_enabled{
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable       = true,
		.depthWriteEnable      = true,
		.depthCompareOp        = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = false,
		.stencilTestEnable     = false
	};

	const VkPipelineColorBlendAttachmentState default_color_blend_state{
		.blendEnable         = true,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp        = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp        = VK_BLEND_OP_ADD,
		.colorWriteMask
		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineMultisampleStateCreateInfo multisample_state_disabled{
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
}