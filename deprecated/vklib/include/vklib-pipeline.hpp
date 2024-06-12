#pragma once

#include "vklib-device.hpp"
#include "vklib-renderpass.hpp"
#include "vklib-shader.hpp"

namespace vklib
{
	class Pipeline_layout : public Base_copyable_parented<VkPipelineLayout, Device>
	{
	  public:

		using Base_copyable_parented<VkPipelineLayout, Device>::Base_copyable_parented;

		// TODO: Add customized layout
		static Result<Pipeline_layout, VkResult> create(
			const Device&                    device,
			std::span<VkDescriptorSetLayout> descriptor_set_layouts = {},
			std::span<VkPushConstantRange>   push_const_range       = {}
		);

		void clean() override;
		~Pipeline_layout() override { clean(); }
	};

	class Graphics_pipeline : public Base_copyable_parented<VkPipeline, Device>
	{
		using Base_copyable_parented<VkPipeline, Device>::Base_copyable_parented;

	  public:

		static Result<Graphics_pipeline, VkResult> create(
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
			VkPipeline                                           base_pipeline       = VK_NULL_HANDLE,
			int32_t                                              base_pipeline_index = -1
		);

		void clean() override;
		~Graphics_pipeline() override { clean(); };
	};
}