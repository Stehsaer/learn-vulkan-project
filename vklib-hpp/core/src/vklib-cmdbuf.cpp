#include "vklib-cmdbuf.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	Command_buffer::Command_buffer(const Command_pool& pool, vk::CommandBufferLevel level)
	{
		const vk::CommandBufferAllocateInfo alloc_info(pool, level, 1);

		const auto result = pool.parent()->allocateCommandBuffers(alloc_info)[0];

		*this = Command_buffer(result, pool);
	}

	std::vector<Command_buffer> Command_buffer::allocate_multiple_from(
		const Command_pool&    pool,
		uint32_t               count,
		vk::CommandBufferLevel level
	)
	{
		const vk::CommandBufferAllocateInfo alloc_info(pool, level, count);

		const auto result = pool.parent()->allocateCommandBuffers(alloc_info);

		std::vector<Command_buffer> moved_result;
		moved_result.reserve(result.size());

		for (const auto& buf : result) moved_result.emplace_back(Command_buffer(buf, pool));

		return moved_result;
	}

	void Command_buffer::clean()
	{
		if (is_unique()) parent().parent()->freeCommandBuffers(parent(), data->child);
	}

	/* Command Buffer Functions */

	vk::Result Command_buffer::begin(bool one_time_submit) const
	{
		const vk::CommandBufferBeginInfo begin_info(
			one_time_submit ? vk::CommandBufferUsageFlagBits::eOneTimeSubmit
							: vk::CommandBufferUsageFlagBits::eSimultaneousUse
		);

		return data->child.begin(&begin_info);
	}

	void Command_buffer::begin_render_pass(
		const Render_pass&                render_pass,
		const Framebuffer&                framebuffer,
		const vk::Rect2D&                 render_area,
		Const_array_proxy<vk::ClearValue> clear_values,
		vk::SubpassContents               contents
	) const
	{
		data->child.beginRenderPass(
			vk::RenderPassBeginInfo()
				.setRenderPass(render_pass)
				.setFramebuffer(framebuffer)
				.setRenderArea(render_area)
				.setClearValues(clear_values),
			contents
		);
	}

	void Command_buffer::layout_transit(
		vk::Image                 image,
		vk::ImageLayout           old_layout,
		vk::ImageLayout           new_layout,
		vk::AccessFlags           src_access_flags,
		vk::AccessFlags           dst_access_flags,
		vk::PipelineStageFlags    src_stage,
		vk::PipelineStageFlags    dst_stage,
		vk::ImageSubresourceRange subresource_range,
		vk::DependencyFlags       flags,
		uint32_t                  src_queue_family_idx,
		uint32_t                  dst_queue_family_idx
	) const
	{
		const auto image_barrier = vk::ImageMemoryBarrier()
									   .setImage(image)
									   .setOldLayout(old_layout)
									   .setNewLayout(new_layout)
									   .setSrcAccessMask(src_access_flags)
									   .setDstAccessMask(dst_access_flags)
									   .setSubresourceRange(subresource_range)
									   .setSrcQueueFamilyIndex(src_queue_family_idx)
									   .setDstQueueFamilyIndex(dst_queue_family_idx);

		data->child.pipelineBarrier(src_stage, dst_stage, flags, {}, {}, image_barrier);
	}
}