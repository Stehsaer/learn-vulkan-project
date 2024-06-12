#pragma once

#include "vklib-buffer.hpp"
#include "vklib-descriptor.hpp"
#include "vklib-device.hpp"
#include "vklib-framebuffer.hpp"
#include "vklib-image.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-renderpass.hpp"

namespace vklib
{

	class Command_pool : public Base_copyable_parented<VkCommandPool, Device>
	{
		using Base_copyable_parented<VkCommandPool, Device>::Base_copyable_parented;

	  public:

		static Result<Command_pool, VkResult> create(
			const Device&            device,
			uint32_t                 queue_family,
			VkCommandPoolCreateFlags flags = 0
		);

		void clean() override;
		~Command_pool() override;

		void free(VkCommandBuffer cmd_buffer);
	};

	class Command_buffer : public Base_copyable_parented<VkCommandBuffer, Command_pool>
	{
	  private:

		using Base_copyable_parented<VkCommandBuffer, Command_pool>::Base_copyable_parented;

		Command_buffer(const Command_pool& cmd_pool, VkCommandBuffer buf) :
			Base_copyable_parented<VkCommandBuffer, Command_pool>(buf, cmd_pool),
			cmd(buf)
		{
		}

		/* Commands */

		class Commands_wrapper
		{
			VkCommandBuffer buf;

		  public:

			Commands_wrapper(VkCommandBuffer buf) :
				buf(buf)
			{
			}

			void begin_render_pass(
				const Render_pass&      render_pass,
				const Framebuffer&      framebuffer,
				VkRect2D                render_area,
				std::span<VkClearValue> clear_colors,
				VkSubpassContents       subpass_content = VK_SUBPASS_CONTENTS_INLINE
			);
			void end_render_pass();

			void bind_graphics_pipeline(const Graphics_pipeline& pipeline);
			void bind_vertex_buffers(
				uint32_t                first_bind,
				uint32_t                bind_count,
				std::span<VkBuffer>     buffers,
				std::span<VkDeviceSize> device_memories
			) const;
			void bind_index_buffer(
				const Buffer& idx_buffer,
				VkDeviceSize  offset,
				VkIndexType   idx_type = VK_INDEX_TYPE_UINT16
			) const;
			void bind_descriptor_sets(
				const Pipeline_layout&     pipeline_layout,
				VkPipelineBindPoint        bind_point,
				uint32_t                   first_set,
				std::span<VkDescriptorSet> descriptor_sets
			) const;

			void draw(uint32_t first_vertex, uint32_t vertex_count, uint32_t first_instance, uint32_t instance_count)
				const;
			void draw_indexed(
				uint32_t first_index,
				uint32_t index_count,
				uint32_t first_instance,
				uint32_t instance_count,
				int32_t  vertex_offset
			) const;

			void set_viewport(std::span<VkViewport> viewports);
			void set_scissor(std::span<VkRect2D> scissors);

			void copy_buffer(
				const Buffer& dst,
				const Buffer& src,
				size_t        size,
				size_t        dst_offset = 0,
				size_t        src_offset = 0
			) const;
			void copy_buffer(const Buffer& dst, const Buffer& src) const
			{
				copy_buffer(dst, src, src->allocation.info.size);
			}
			void copy_image(
				const Image&                 dst,
				const Buffer&                src,
				VkImageLayout                dst_image_layout,
				std::span<VkBufferImageCopy> regions
			) const;

			void image_layout_transit(
				const Image&            image,
				VkImageLayout           old_layout,
				VkImageLayout           new_layout,
				VkAccessFlags           src_access_flags,
				VkAccessFlags           dst_access_flags,
				VkPipelineStageFlags    src_stage,
				VkPipelineStageFlags    dst_stage,
				VkImageSubresourceRange subresource_range,
				uint32_t                src_queue_family_idx = VK_QUEUE_FAMILY_IGNORED,
				uint32_t                dst_queue_family_idx = VK_QUEUE_FAMILY_IGNORED
			) const;
			void blit_image(
				const Image&           dst,
				const Image&           src,
				VkImageLayout          dst_layout,
				VkImageLayout          src_layout,
				VkFilter               filter,
				std::span<VkImageBlit> blit_regions
			) const;
		};

	  public:

		/* Overriding Default Constructor */

		Command_buffer() :
			cmd(nullptr)
		{
		}

		/* Create */

		static Result<std::vector<Command_buffer>, VkResult> allocate_multiple_from(
			const Command_pool&  cmd_pool,
			VkCommandBufferLevel level,
			uint32_t             count
		);

		static Result<Command_buffer, VkResult> allocate_from(const Command_pool& cmd_pool, VkCommandBufferLevel level);

		/* Control */

		void     reset(VkCommandBufferResetFlagBits flag = (VkCommandBufferResetFlagBits)0);
		VkResult begin(VkCommandBufferUsageFlagBits usage = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
		VkResult begin_single_submit() { return begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); }
		VkResult end();

		/* Destroy */

		void clean() override;
		~Command_buffer() override { clean(); }

		/* Command_wrapper object */

		Commands_wrapper cmd;
	};
}