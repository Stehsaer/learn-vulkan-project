#include "vklib-pipeline.hpp"

namespace VKLIB_HPP_NAMESPACE
{
#pragma region "Descriptor Pool"

	Descriptor_pool::Descriptor_pool(const Device& device, Array_proxy<vk::DescriptorPoolSize> pool_sizes, uint32_t max_sets)
	{
		vk::DescriptorPoolCreateInfo create_info;
		create_info.setPoolSizes(pool_sizes)
			.setMaxSets(max_sets)
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

		auto handle = device->createDescriptorPool(create_info);
		*this       = Descriptor_pool(handle, device);
	}

	void Descriptor_pool::clean()
	{
		if (is_unique()) parent()->destroyDescriptorPool(*this);
	}

#pragma endregion

#pragma region "Descriptor Set Layout"

	Descriptor_set_layout::Descriptor_set_layout(const Device& device, Array_proxy<vk::DescriptorSetLayoutBinding> bindings)
	{
		vk::DescriptorSetLayoutCreateInfo create_info;
		create_info.setBindings(bindings);

		auto handle = device->createDescriptorSetLayout(create_info);
		*this       = Descriptor_set_layout(handle, device);
	}

	void Descriptor_set_layout::clean()
	{
		if (is_unique()) parent()->destroyDescriptorSetLayout(*this);
	}

#pragma endregion

#pragma region "Pipeline Layout"

	Pipeline_layout::Pipeline_layout(
		const Device&                        device,
		Array_proxy<vk::DescriptorSetLayout> descriptor_set_layouts,
		Array_proxy<vk::PushConstantRange>   push_constant_ranges
	)
	{
		vk::PipelineLayoutCreateInfo create_info;
		create_info.setSetLayouts(descriptor_set_layouts).setPushConstantRanges(push_constant_ranges);

		auto handle = device->createPipelineLayout(create_info);
		*this       = Pipeline_layout(handle, device);
	}

	void Pipeline_layout::clean()
	{
		if (is_unique()) parent()->destroyPipelineLayout(*this);
	}

#pragma endregion

#pragma region "Descriptor Set"

	std::vector<Descriptor_set> Descriptor_set::create_multiple(
		const Device&                        device,
		const Descriptor_pool&               descriptor_pool,
		Array_proxy<vk::DescriptorSetLayout> layouts
	)
	{
		std::vector<Descriptor_set> target;

		vk::DescriptorSetAllocateInfo allocate_info;
		allocate_info.setDescriptorPool(descriptor_pool).setSetLayouts(layouts).setDescriptorSetCount(layouts.size());

		auto c_handles = device->allocateDescriptorSets(allocate_info);

		for (auto handle : c_handles) target.push_back(Descriptor_set(handle, descriptor_pool));

		return target;
	}

	void Descriptor_set::clean()
	{
		auto clean_array = std::to_array<vk::DescriptorSet>({*this});
		if (is_unique()) parent().parent()->freeDescriptorSets(parent(), clean_array);
	}

#pragma endregion

#pragma region "Shader Module"

	Shader_module::Shader_module(const Device& device, Array_proxy<uint8_t> code)
	{
		auto create_info = vk::ShaderModuleCreateInfo().setCodeSize(code.size()).setPCode((const uint32_t*)code.data());

		auto handle = device->createShaderModule(create_info);
		*this       = Shader_module(handle, device);
	}

	void Shader_module::clean()
	{
		if (is_unique()) parent()->destroyShaderModule(*this);
	}

	vk::PipelineShaderStageCreateInfo Shader_module::stage_info(vk::ShaderStageFlagBits stage, const char* entry_name)
		const
	{
		return vk::PipelineShaderStageCreateInfo().setModule(*this).setStage(stage).setPName(entry_name);
	}

#pragma endregion

#pragma region "Render Pass"

	Render_pass::Render_pass(
		const Device&                          device,
		Array_proxy<vk::AttachmentDescription> attachment_descriptions,
		Array_proxy<vk::SubpassDescription>    subpass_descriptions,
		Array_proxy<vk::SubpassDependency>     subpass_dependencies
	)
	{
		vk::RenderPassCreateInfo create_info;
		create_info.setAttachments(attachment_descriptions)
			.setSubpasses(subpass_descriptions)
			.setDependencies(subpass_dependencies);

		auto handle = device->createRenderPass(create_info);
		*this       = Render_pass(handle, device);
	}

	void Render_pass::clean()
	{
		if (is_unique()) parent()->destroyRenderPass(*this);
	}

#pragma endregion

#pragma region "Pipeline"

	Graphics_pipeline::Graphics_pipeline(const Device& device, const vk::GraphicsPipelineCreateInfo& create_info)
	{
		auto handle = device->createGraphicsPipeline(nullptr, create_info);
		vk::resultCheck(handle.result, "Failed to create pipeline");

		*this = Graphics_pipeline(handle.value, device);
	}

	void Graphics_pipeline::clean()
	{
		if (is_unique()) parent()->destroyPipeline(*this);
	}

	Compute_pipeline::Compute_pipeline(
		const Device&                            device,
		const Pipeline_layout&                   pipeline_layout,
		const vk::PipelineShaderStageCreateInfo& shader_stage
	)
	{
		auto handle
			= device->createComputePipeline(nullptr, vk::ComputePipelineCreateInfo({}, shader_stage, pipeline_layout));
		vk::resultCheck(handle.result, "Failed to create compute pipeline");

		*this = Compute_pipeline(handle.value, device);
	}

	void Compute_pipeline::clean()
	{
		if (is_unique()) parent()->destroyPipeline(*this);
	}

#pragma endregion

#pragma region "Framebuffer"

	Framebuffer::Framebuffer(
		const Device&              device,
		const Render_pass&         render_pass,
		Array_proxy<vk::ImageView> image_views,
		vk::Extent3D               extent
	)
	{
		vk::FramebufferCreateInfo create_info;
		create_info.setRenderPass(render_pass)
			.setAttachments(image_views)
			.setWidth(extent.width)
			.setHeight(extent.height)
			.setLayers(extent.depth);

		auto handle = device->createFramebuffer(create_info);
		*this       = Framebuffer(handle, device);
	}

	void Framebuffer::clean()
	{
		if (is_unique()) parent()->destroyFramebuffer(*this);
	}

#pragma endregion
}