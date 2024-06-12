#include "vklib-command-pool.hpp"

namespace vklib
{
	Result<Command_pool, VkResult> Command_pool::create(
		const Device&            device,
		uint32_t                 queue_family,
		VkCommandPoolCreateFlags flags
	)
	{
		VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		create_info.queueFamilyIndex        = queue_family;
		create_info.flags                   = flags;

		VkCommandPool handle;
		auto          result = vkCreateCommandPool(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Command_pool(handle, device);
	}

	Command_pool::~Command_pool()
	{
		clean();
	}

	void Command_pool::clean()
	{
		if (should_delete()) vkDestroyCommandPool(*parent, *content, parent.allocator_ptr());
	}

	void Command_pool::free(VkCommandBuffer cmd_buffer)
	{
		vkFreeCommandBuffers(*parent, *content, 1, &cmd_buffer);
	}
}

namespace vklib
{
	Result<std::vector<Command_buffer>, VkResult> Command_buffer::allocate_multiple_from(
		const Command_pool&  cmd_pool,
		VkCommandBufferLevel level,
		uint32_t             count
	)
	{
		VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		alloc_info.commandPool        = *cmd_pool;
		alloc_info.level              = level;
		alloc_info.commandBufferCount = count;

		std::vector<VkCommandBuffer> c_buffer(count);

		auto result = vkAllocateCommandBuffers(*(cmd_pool.get_parent()), &alloc_info, c_buffer.data());

		std::vector<Command_buffer> buffers;
		buffers.reserve(count);

		for (size_t i = 0; i < count; i++) buffers.push_back(Command_buffer(cmd_pool, c_buffer[i]));

		if (result != VK_SUCCESS)
			return result;
		else
			return buffers;
	}

	Result<Command_buffer, VkResult> Command_buffer::allocate_from(
		const Command_pool&  cmd_pool,
		VkCommandBufferLevel level
	)
	{
		VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		alloc_info.commandPool        = *cmd_pool;
		alloc_info.level              = level;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer handle;
		auto            result = vkAllocateCommandBuffers(*(cmd_pool.get_parent()), &alloc_info, &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Command_buffer(cmd_pool, handle);
	}

	VkResult Command_buffer::begin(VkCommandBufferUsageFlagBits usage)
	{
		VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		info.flags = usage;
		return vkBeginCommandBuffer(*content, &info);
	}

	void Command_buffer::Commands_wrapper::begin_render_pass(
		const Render_pass&      render_pass,
		const Framebuffer&      framebuffer,
		VkRect2D                render_area,
		std::span<VkClearValue> clear_colors,
		VkSubpassContents       subpass_content
	)
	{
		VkRenderPassBeginInfo begin_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		begin_info.renderPass      = *render_pass;
		begin_info.framebuffer     = *framebuffer;
		begin_info.renderArea      = render_area;
		begin_info.clearValueCount = clear_colors.size();
		begin_info.pClearValues    = clear_colors.data();

		vkCmdBeginRenderPass(buf, &begin_info, subpass_content);
	}

	void Command_buffer::Commands_wrapper::end_render_pass()
	{
		vkCmdEndRenderPass(buf);
	}

	void Command_buffer::Commands_wrapper::bind_graphics_pipeline(const Graphics_pipeline& pipeline)
	{
		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	}

	void Command_buffer::Commands_wrapper::draw(
		uint32_t first_vertex,
		uint32_t vertex_count,
		uint32_t first_instance,
		uint32_t instance_count
	) const
	{
		vkCmdDraw(buf, vertex_count, instance_count, first_vertex, first_instance);
	}

	void Command_buffer::Commands_wrapper::draw_indexed(
		uint32_t first_index,
		uint32_t index_count,
		uint32_t first_instance,
		uint32_t instance_count,
		int32_t  vertex_offset
	) const
	{
		vkCmdDrawIndexed(buf, index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void Command_buffer::Commands_wrapper::bind_vertex_buffers(
		uint32_t                first_bind,
		uint32_t                bind_count,
		std::span<VkBuffer>     buffers,
		std::span<VkDeviceSize> device_memories
	) const
	{
		assert(bind_count == buffers.size() && bind_count == device_memories.size());
		vkCmdBindVertexBuffers(buf, first_bind, bind_count, buffers.data(), device_memories.data());
	}

	void Command_buffer::Commands_wrapper::bind_index_buffer(
		const Buffer& idx_buffer,
		VkDeviceSize  offset,
		VkIndexType   idx_type
	) const
	{
		vkCmdBindIndexBuffer(buf, idx_buffer->buffer, offset, idx_type);
	}

	void Command_buffer::Commands_wrapper::bind_descriptor_sets(
		const Pipeline_layout&     pipeline_layout,
		VkPipelineBindPoint        bind_point,
		uint32_t                   first_set,
		std::span<VkDescriptorSet> descriptor_sets
	) const
	{
		vkCmdBindDescriptorSets(
			buf,
			bind_point,
			*pipeline_layout,
			first_set,
			descriptor_sets.size(),
			descriptor_sets.data(),
			0,
			nullptr
		);
	}

	void Command_buffer::Commands_wrapper::set_viewport(std::span<VkViewport> viewports)
	{
		vkCmdSetViewport(buf, 0, viewports.size(), viewports.data());
	}

	void Command_buffer::Commands_wrapper::set_scissor(std::span<VkRect2D> scissors)
	{
		vkCmdSetScissor(buf, 0, scissors.size(), scissors.data());
	}

	void Command_buffer::Commands_wrapper::copy_buffer(
		const Buffer& dst,
		const Buffer& src,
		size_t        size,
		size_t        dst_offset,
		size_t        src_offset
	) const
	{
		const VkBufferCopy copy_region{.srcOffset = src_offset, .dstOffset = dst_offset, .size = size};

		vkCmdCopyBuffer(buf, src->buffer, dst->buffer, 1, &copy_region);
	}

	void Command_buffer::Commands_wrapper::copy_image(
		const Image&                 dst,
		const Buffer&                src,
		VkImageLayout                dst_image_layout,
		std::span<VkBufferImageCopy> regions
	) const
	{
		vkCmdCopyBufferToImage(buf, src->buffer, dst->image, dst_image_layout, regions.size(), regions.data());
	}

	void Command_buffer::Commands_wrapper::image_layout_transit(
		const Image&            image,
		VkImageLayout           old_layout,
		VkImageLayout           new_layout,
		VkAccessFlags           src_access_flags,
		VkAccessFlags           dst_access_flags,
		VkPipelineStageFlags    src_stage,
		VkPipelineStageFlags    dst_stage,
		VkImageSubresourceRange subresource_range,
		uint32_t                src_queue_family_idx,
		uint32_t                dst_queue_family_idx
	) const
	{
		VkImageMemoryBarrier image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		image_barrier.image               = image->image;
		image_barrier.subresourceRange    = subresource_range;
		image_barrier.srcAccessMask       = src_access_flags;
		image_barrier.dstAccessMask       = dst_access_flags;
		image_barrier.srcQueueFamilyIndex = src_queue_family_idx;
		image_barrier.dstQueueFamilyIndex = dst_queue_family_idx;
		image_barrier.oldLayout           = old_layout;
		image_barrier.newLayout           = new_layout;

		vkCmdPipelineBarrier(buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
	}

	void Command_buffer::Commands_wrapper::blit_image(
		const Image&           dst,
		const Image&           src,
		VkImageLayout          dst_layout,
		VkImageLayout          src_layout,
		VkFilter               filter,
		std::span<VkImageBlit> blit_regions
	) const
	{
		vkCmdBlitImage(
			buf,
			src->image,
			src_layout,
			dst->image,
			dst_layout,
			blit_regions.size(),
			blit_regions.data(),
			filter
		);
	}

	VkResult Command_buffer::end()
	{
		return vkEndCommandBuffer(*content);
	}

	void Command_buffer::reset(VkCommandBufferResetFlagBits flags)
	{
		vkResetCommandBuffer(*content, flags);
	}

	void Command_buffer::clean()
	{
		if (should_delete()) vkFreeCommandBuffers(*(parent.get_parent()), *parent, 1, content.get());
	}
}