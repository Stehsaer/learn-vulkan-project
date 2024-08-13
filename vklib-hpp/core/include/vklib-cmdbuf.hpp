#pragma once

#include "vklib-base.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-storage.hpp"

namespace VKLIB_HPP_NAMESPACE
{
	struct Image_layout_state
	{
		vk::ImageLayout layout;
		vk::AccessFlags access_flags;
		uint32_t        queue_family_idx = vk::QueueFamilyIgnored;
	};

	inline vk::ImageMemoryBarrier image_layout_transit(
		const Image&              image,
		Image_layout_state        old_state,
		Image_layout_state        new_state,
		vk::ImageSubresourceRange subresource_range
	)
	{
		return {
			old_state.access_flags,
			new_state.access_flags,
			old_state.layout,
			new_state.layout,
			old_state.queue_family_idx,
			new_state.queue_family_idx,
			image,
			subresource_range
		};
	}

	class Command_buffer : public Child_resource<vk::CommandBuffer, Command_pool>
	{
		using Child_resource<vk::CommandBuffer, Command_pool>::Child_resource;

		void clean() override;

	  public:

		Command_buffer(const Command_pool& pool, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

		static std::vector<Command_buffer> allocate_multiple_from(
			const Command_pool&    pool,
			uint32_t               count,
			vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary
		);

		~Command_buffer() override { clean(); }

		/*==== Helpers & Functions ====*/

		/* State Transition */

		vk::Result begin(bool one_time_submit = false) const;

		inline void end() const { data->child.end(); }
		inline void reset() const { data->child.reset(); }

		inline void begin_render_pass(
			const Render_pass&          render_pass,
			const Framebuffer&          framebuffer,
			const vk::Rect2D&           render_area,
			Array_proxy<vk::ClearValue> clear_values,
			vk::SubpassContents         contents = vk::SubpassContents::eInline
		) const;

		inline void next_subpass(vk::SubpassContents contents = vk::SubpassContents::eInline) const
		{
			data->child.nextSubpass(contents);
		}

		inline void end_render_pass() const { data->child.endRenderPass(); }

		/* Draw */

		inline void draw(uint32_t first_vertex, uint32_t vertex_count, uint32_t first_instance, uint32_t instance_count) const
		{
			data->child.draw(vertex_count, instance_count, first_vertex, first_instance);
		}

		inline void set_viewport(const vk::Viewport& viewport) const { data->child.setViewport(0, viewport); }
		inline void set_scissor(const vk::Rect2D& scissor) const { data->child.setScissor(0, scissor); }

		/* Binding */

		inline void bind_descriptor_sets(
			vk::PipelineBindPoint          bind_point,
			const Pipeline_layout&         pipeline_layout,
			uint32_t                       bind_offset,
			Array_proxy<vk::DescriptorSet> descriptor_sets
		) const
		{
			data->child.bindDescriptorSets(bind_point, pipeline_layout, bind_offset, descriptor_sets, {});
		}

		inline void bind_pipeline(vk::PipelineBindPoint bind_point, const Child_resource<vk::Pipeline, Device>& graphics_pipeline)
			const
		{
			data->child.bindPipeline(bind_point, graphics_pipeline);
		}

		inline void bind_vertex_buffers(
			uint32_t                    first_binding,
			Array_proxy<vk::Buffer>     vertex_buffer,
			Array_proxy<vk::DeviceSize> offsets
		) const
		{
			data->child.bindVertexBuffers(first_binding, vertex_buffer, offsets);
		}

		inline void push_constants(
			const Pipeline_layout& pipeline_layout,
			vk::ShaderStageFlags   shader_stage,
			uint32_t               offset,
			uint32_t               size,
			const void*            data
		) const
		{
			this->data->child.pushConstants(pipeline_layout, shader_stage, offset, size, data);
		}

		template <typename T>
		inline void push_constants(
			const Pipeline_layout& pipeline_layout,
			vk::ShaderStageFlags   shader_stage,
			const T&               obj,
			uint32_t               offset = 0
		) const
		{
			push_constants(pipeline_layout, shader_stage, offset, sizeof(std::remove_cvref_t<T>), &obj);
		}

		/* Memory Manipulation*/

		inline void copy_buffer(const Buffer& dst_buffer, const Buffer& src_buffer, size_t dst_offset, size_t src_offset, size_t size)
			const
		{
			data->child.copyBuffer(src_buffer, dst_buffer, vk::BufferCopy(src_offset, dst_offset, size));
		}

		inline void copy_buffer_to_image(
			const Image&                     dst_image,
			const Buffer&                    src_buffer,
			vk::ImageLayout                  image_layout,
			Array_proxy<vk::BufferImageCopy> copy_region
		) const
		{
			data->child.copyBufferToImage(src_buffer, dst_image, image_layout, copy_region);
		}

		inline void blit_image(
			vk::Image                  src_image,
			vk::ImageLayout            src_image_layout,
			vk::Image                  dst_image,
			vk::ImageLayout            dst_image_layout,
			Array_proxy<vk::ImageBlit> regions,
			vk::Filter                 blit_filter
		) const
		{
			data->child.blitImage(src_image, src_image_layout, dst_image, dst_image_layout, regions, blit_filter);
		}

		inline void pipeline_barrier(
			vk::PipelineStageFlags               src_stage,
			vk::PipelineStageFlags               dst_stage,
			Array_proxy<vk::MemoryBarrier>       memory_barriers,
			Array_proxy<vk::BufferMemoryBarrier> buffer_barriers,
			Array_proxy<vk::ImageMemoryBarrier>  image_barriers,
			vk::DependencyFlags                  dependency_flags = {}
		) const
		{
			data->child.pipelineBarrier(src_stage, dst_stage, dependency_flags, memory_barriers, buffer_barriers, image_barriers);
		}

		void layout_transit(
			vk::Image                 image,
			vk::ImageLayout           old_layout,
			vk::ImageLayout           new_layout,
			vk::AccessFlags           src_access_flags,
			vk::AccessFlags           dst_access_flags,
			vk::PipelineStageFlags    src_stage,
			vk::PipelineStageFlags    dst_stage,
			vk::ImageSubresourceRange subresource_range,
			vk::DependencyFlags       flags                = {},
			uint32_t                  src_queue_family_idx = vk::QueueFamilyIgnored,
			uint32_t                  dst_queue_family_idx = vk::QueueFamilyIgnored
		) const;
	};
}