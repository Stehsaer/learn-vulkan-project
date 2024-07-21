#pragma once

#include "vklib-base.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-storage.hpp"

namespace VKLIB_HPP_NAMESPACE
{
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
		void       end() const { data->child.end(); }
		void       reset() const { data->child.reset(); }

		void begin_render_pass(
			const Render_pass&                render_pass,
			const Framebuffer&                framebuffer,
			const vk::Rect2D&                 render_area,
			Const_array_proxy<vk::ClearValue> clear_values,
			vk::SubpassContents               contents = vk::SubpassContents::eInline
		) const;

		void next_subpass(vk::SubpassContents contents = vk::SubpassContents::eInline) const
		{
			data->child.nextSubpass(contents);
		}

		void end_render_pass() const { data->child.endRenderPass(); }

		/* Draw */

		void draw(uint32_t first_vertex, uint32_t vertex_count, uint32_t first_instance, uint32_t instance_count) const
		{
			data->child.draw(vertex_count, instance_count, first_vertex, first_instance);
		}

		void set_viewport(const vk::Viewport& viewport) const { data->child.setViewport(0, viewport); }
		void set_scissor(const vk::Rect2D& scissor) const { data->child.setScissor(0, scissor); }

		/* Binding */

		void bind_descriptor_sets(
			vk::PipelineBindPoint                bind_point,
			const Pipeline_layout&               pipeline_layout,
			uint32_t                             bind_offset,
			Const_array_proxy<vk::DescriptorSet> descriptor_sets
		) const
		{
			data->child.bindDescriptorSets(bind_point, pipeline_layout, bind_offset, descriptor_sets, {});
		}

		void bind_pipeline(
			vk::PipelineBindPoint                       bind_point,
			const Child_resource<vk::Pipeline, Device>& graphics_pipeline
		) const
		{
			data->child.bindPipeline(bind_point, graphics_pipeline);
		}

		void bind_vertex_buffers(
			uint32_t                          first_binding,
			Const_array_proxy<vk::Buffer>     vertex_buffer,
			Const_array_proxy<vk::DeviceSize> offsets
		) const
		{
			data->child.bindVertexBuffers(first_binding, vertex_buffer, offsets);
		}

		void push_constants(
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
		void push_constants(
			const Pipeline_layout& pipeline_layout,
			vk::ShaderStageFlags   shader_stage,
			const T&               obj,
			uint32_t               offset = 0
		) const
		{
			push_constants(pipeline_layout, shader_stage, offset, sizeof(std::remove_cvref_t<T>), &obj);
		}

		/* Memory Manipulation*/

		void copy_buffer(
			const Buffer& dst_buffer,
			const Buffer& src_buffer,
			size_t        dst_offset,
			size_t        src_offset,
			size_t        size
		) const
		{
			data->child.copyBuffer(src_buffer, dst_buffer, vk::BufferCopy(src_offset, dst_offset, size));
		}

		void copy_buffer_to_image(
			const Image&                           dst_image,
			const Buffer&                          src_buffer,
			vk::ImageLayout                        image_layout,
			Const_array_proxy<vk::BufferImageCopy> copy_region
		) const
		{
			data->child.copyBufferToImage(src_buffer, dst_image, image_layout, copy_region);
		}

		void blit_image(
			vk::Image                        src_image,
			vk::ImageLayout                  src_image_layout,
			vk::Image                        dst_image,
			vk::ImageLayout                  dst_image_layout,
			Const_array_proxy<vk::ImageBlit> regions,
			vk::Filter                       blit_filter
		) const
		{
			data->child.blitImage(src_image, src_image_layout, dst_image, dst_image_layout, regions, blit_filter);
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