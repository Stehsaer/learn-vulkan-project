#pragma once

#include "vklib-env.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	class Descriptor_pool : public Child_resource<vk::DescriptorPool, Device>
	{
		using Child_resource<vk::DescriptorPool, Device>::Child_resource;

		void clean() override;

	  public:

		Descriptor_pool(const Device& device, Const_array_proxy<vk::DescriptorPoolSize> pool_sizes, uint32_t max_sets);

		~Descriptor_pool() override { clean(); }
	};

	class Descriptor_set_layout : public Child_resource<vk::DescriptorSetLayout, Device>
	{
		using Child_resource<vk::DescriptorSetLayout, Device>::Child_resource;

		void clean() override;

	  public:

		Descriptor_set_layout(const Device& device, Const_array_proxy<vk::DescriptorSetLayoutBinding> bindings);

		~Descriptor_set_layout() override { clean(); }
	};

	class Pipeline_layout : public Child_resource<vk::PipelineLayout, Device>
	{
		using Child_resource<vk::PipelineLayout, Device>::Child_resource;

		void clean() override;

	  public:

		Pipeline_layout(
			const Device&                              device,
			Const_array_proxy<vk::DescriptorSetLayout> descriptor_set_layouts,
			Const_array_proxy<vk::PushConstantRange>   push_constant_ranges
		);

		~Pipeline_layout() override { clean(); }
	};

	class Descriptor_set : public Child_resource<vk::DescriptorSet, Descriptor_pool>
	{
		using Child_resource<vk::DescriptorSet, Descriptor_pool>::Child_resource;

		void clean() override;

	  public:

		static std::vector<Descriptor_set> create_multiple(
			const Device&                              device,
			const Descriptor_pool&                     descriptor_pool,
			Const_array_proxy<vk::DescriptorSetLayout> layouts
		);

		~Descriptor_set() override { clean(); }
	};

	class Shader_module : public Child_resource<vk::ShaderModule, Device>
	{
		using Child_resource<vk::ShaderModule, Device>::Child_resource;

		void clean() override;

	  public:

		Shader_module(const Device& device, Const_array_proxy<uint8_t> code);

		~Shader_module() override { clean(); }

		vk::PipelineShaderStageCreateInfo stage_info(vk::ShaderStageFlagBits stage, const char* entry_name = "main")
			const;
	};

	class Render_pass : public Child_resource<vk::RenderPass, Device>
	{
		using Child_resource<vk::RenderPass, Device>::Child_resource;

		void clean() override;

	  public:

		Render_pass(
			const Device&                                device,
			Const_array_proxy<vk::AttachmentDescription> attachment_descriptions,
			Const_array_proxy<vk::SubpassDescription>    subpass_descriptions,
			Const_array_proxy<vk::SubpassDependency>     subpass_dependencies
		);

		~Render_pass() override { clean(); }
	};

	using Pipeline_base = Child_resource<vk::Pipeline, Device>;

	class Graphics_pipeline : public Pipeline_base
	{
		using Pipeline_base::Child_resource;

		void clean() override;

	  public:

		Graphics_pipeline(const Device& device, const vk::GraphicsPipelineCreateInfo& create_info);

		~Graphics_pipeline() override { clean(); }
	};

	class Compute_pipeline : public Pipeline_base
	{
		using Pipeline_base::Child_resource;

		void clean() override;

	  public:

		Compute_pipeline(
			const Device&                            device,
			const Pipeline_layout&                   pipeline_layout,
			const vk::PipelineShaderStageCreateInfo& shader_stage
		);

		~Compute_pipeline() override { clean(); }
	};

	class Framebuffer : public Child_resource<vk::Framebuffer, Device>
	{
		using Child_resource<vk::Framebuffer, Device>::Child_resource;

		void clean() override;

	  public:

		Framebuffer(
			const Device&                    device,
			const Render_pass&               render_pass,
			Const_array_proxy<vk::ImageView> image_views,
			vk::Extent3D                     extent
		);

		~Framebuffer() override { clean(); }
	};
}