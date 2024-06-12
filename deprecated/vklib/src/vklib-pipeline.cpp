#include "vklib-pipeline.hpp"

// Pipeline Layout
namespace vklib
{
	Result<Pipeline_layout, VkResult> Pipeline_layout::create(
		const Device&                    device,
		std::span<VkDescriptorSetLayout> descriptor_set_layouts,
		std::span<VkPushConstantRange>   push_const_range
	)
	{
		VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		create_info.setLayoutCount             = descriptor_set_layouts.size();
		create_info.pSetLayouts                = descriptor_set_layouts.data();
		create_info.pushConstantRangeCount     = push_const_range.size();
		create_info.pPushConstantRanges        = push_const_range.data();

		VkPipelineLayout layout;
		VkResult         result = vkCreatePipelineLayout(*device, &create_info, nullptr, &layout);

		if (result != VK_SUCCESS) return result;

		return Pipeline_layout(layout, device);
	}

	void Pipeline_layout::clean()
	{
		if (should_delete()) vkDestroyPipelineLayout(*parent, *content, parent.allocator_ptr());
	}
}

// Pipeline
namespace vklib
{
	Result<Graphics_pipeline, VkResult> Graphics_pipeline::create(
		const Device&                                        device,
		std::span<VkPipelineShaderStageCreateInfo>           shader_stages,
		const Pipeline_layout&                               layout,
		const Render_pass&                                   render_pass,
		VkPipelineVertexInputStateCreateInfo                 vertex_input_state,
		VkPipelineInputAssemblyStateCreateInfo               input_assembly_state,
		VkPipelineViewportStateCreateInfo                    viewport_state,
		VkPipelineRasterizationStateCreateInfo               rasterization_state,
		VkPipelineMultisampleStateCreateInfo                 multisample_state,
		std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_state,
		VkPipelineColorBlendStateCreateInfo                  color_blend_state,
		std::optional<VkPipelineDynamicStateCreateInfo>      dynamic_state,
		std::optional<VkPipelineTessellationStateCreateInfo> tessellation_state,
		VkPipeline                                           base_pipeline,
		int32_t                                              base_pipeline_index
	)
	{
		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount                   = shader_stages.size();
		pipeline_info.pStages                      = shader_stages.data();
		pipeline_info.pVertexInputState            = &vertex_input_state;
		pipeline_info.pInputAssemblyState          = &input_assembly_state;
		pipeline_info.pViewportState               = &viewport_state;
		pipeline_info.pRasterizationState          = &rasterization_state;
		pipeline_info.pMultisampleState            = &multisample_state;
		pipeline_info.pDepthStencilState = depth_stencil_state.has_value() ? &depth_stencil_state.value() : nullptr;
		pipeline_info.pColorBlendState   = &color_blend_state;
		pipeline_info.pDynamicState      = dynamic_state.has_value() ? &dynamic_state.value() : nullptr;
		pipeline_info.pTessellationState = tessellation_state.has_value() ? &tessellation_state.value() : nullptr;
		pipeline_info.layout             = *layout;
		pipeline_info.renderPass         = *render_pass;
		pipeline_info.basePipelineHandle = base_pipeline;
		pipeline_info.basePipelineIndex  = base_pipeline_index;
		pipeline_info.flags              = 0;

		VkPipeline pipeline;
		auto       result
			= vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipeline_info, device.allocator_ptr(), &pipeline);

		if (result != VK_SUCCESS)
			return result;
		else
			return Graphics_pipeline(pipeline, device);
	}

	void Graphics_pipeline::clean()
	{
		if (should_delete()) vkDestroyPipeline(*parent, *content, parent.allocator_ptr());
	}
}