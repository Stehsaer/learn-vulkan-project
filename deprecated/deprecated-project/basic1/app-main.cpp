#include "app.hpp"

#include <chrono>

void Application::run()
{
	while (!window.should_close())
	{
		glfwPollEvents();
		main_draw();
	}

	main_device.wait_idle();
}

void Application::record_command_buffer(uint32_t index)
{

	bind_descriptors(index);

	command_buffer.begin();

	// main draw
	{
		{  // begin render pass
			std::array<VkClearValue, 2> clear_values;
			clear_values[0].color = {
				{0.0, 0.0, 0.0, 0.0}
			};
			clear_values[1].depthStencil = {.depth = 1.0};

			VkRect2D render_area{
				.offset = {0, 0},
				.extent = swap_extent
			};

			command_buffer.cmd.begin_render_pass(render_pass, framebuffers[index], render_area, clear_values);
		}
		command_buffer.cmd.bind_graphics_pipeline(graphics_pipeline);

		{  // bind vertex buffers
			auto bind_vertex_buffers = std::to_array({vertex_buffer->buffer});
			auto bind_device_sizes   = std::to_array<VkDeviceSize>({0});
			command_buffer.cmd.bind_vertex_buffers(0, 1, bind_vertex_buffers, bind_device_sizes);
		}

		command_buffer.cmd.bind_index_buffer(index_buffer, 0);

		{  // bind descriptor sets
			auto target_bind_descriptor_sets = std::to_array({*descriptor_sets[index]});
			command_buffer.cmd
				.bind_descriptor_sets(pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, target_bind_descriptor_sets);
		}

		// Dynamic States
		VkViewport dynamic_viewport{0, 0, (float)swap_extent.width, (float)swap_extent.height, 0.0, 1.0};
		tools::flip_viewport(dynamic_viewport);

		VkRect2D scissor{
			{0, 0},
			swap_extent
		};

		auto viewports = std::to_array({dynamic_viewport});
		auto scissors  = std::to_array({scissor});

		command_buffer.cmd.set_viewport(viewports);
		command_buffer.cmd.set_scissor(scissors);

		// command_buffer.cmd.draw(0, 3, 0, 1);
		command_buffer.cmd.draw_indexed(0, index_list.size(), 0, 1, 0);

		command_buffer.cmd.end_render_pass();
	}

	auto end_result = command_buffer.end();
	check_success(end_result, "Command End failed");
}

void Application::test_recreate_swapchain()
{
	tools::print("Recreating Swapchain");

	// Handling minimized scenario
	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(*window, &width, &height);
		glfwWaitEvents();
	}

	// Note: Swapchain and all its related object must be cleaned before recreating a new one
	for (auto& fb : framebuffers) fb.destroy();
	for (auto& view : swapchain_image_views) view.destroy();
	swapchain.destroy();

	// reset states
	next_frame_fence.reset();
	command_buffer.reset();

	// Recreate
	init_swapchain();
	init_depth_buffer();
	init_pipeline();
	init_descriptors();
}

void Application::bind_descriptors(uint32_t index) const
{
	// Uniform buffer
	static auto start_time = std::chrono::steady_clock::now();
	auto        end_time   = std::chrono::steady_clock::now();
	auto        duration   = std::chrono::duration<float, std::chrono::seconds::period>(end_time - start_time).count();

	Uniform_object ubo;
	ubo.model = glm::rotate(glm::mat4(1), duration, {0.0, 0.0, 1.0});
	ubo.view  = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo.projection = glm::perspective(glm::radians(60.0f), swap_extent.width / (float)swap_extent.height, 0.1f, 10.0f);

	uniform_buffers[index]->allocation.transfer(ubo);

	VkDescriptorBufferInfo buffer_info{
		.buffer = uniform_buffers[index]->buffer,
		.offset = 0,
		.range  = sizeof(Uniform_object)
	};

	VkWriteDescriptorSet uniform_write_info{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = *descriptor_sets[index],
		.dstBinding      = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo     = &buffer_info
	};

	// Image
	VkDescriptorImageInfo texture_info{
		.sampler     = *sampler,
		.imageView   = *texture_image_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet texture_write_info{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = *descriptor_sets[index],
		.dstBinding      = 1,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo      = &texture_info
	};

	auto write_info_list = std::to_array({uniform_write_info, texture_write_info});

	descriptor_sets[index].update(write_info_list, {});
}

void Application::main_draw()
{
	// Wait for fences
	check_success(next_frame_fence.wait(1e9), "Fence wait timeout!");
	next_frame_fence.reset();

	// Acqurie Next Image
	uint32_t next_image_index;

	// Check if swapchain needed to be recreated
	while (true)
	{
		auto acquire_next_image_result
			= swapchain.acquire_next_image(image_available_semaphore, std::nullopt) >> next_image_index;

		if (!acquire_next_image_result)
			if (acquire_next_image_result.err() == VK_ERROR_OUT_OF_DATE_KHR
				|| acquire_next_image_result.err() == VK_SUBOPTIMAL_KHR)
			{
				tools::print("Swapchain unavailable: {}", magic_enum::enum_name(acquire_next_image_result.err()));
				test_recreate_swapchain();
				continue;
			}

		check_success(acquire_next_image_result, "Failed to acquire next image");
		break;
	}

	// Record Command
	command_buffer.reset();
	record_command_buffer(next_image_index);

	// Submit Queue

	auto         wait_semaphores   = {*image_available_semaphore};
	auto         wait_stage_mask   = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	auto         signal_semaphores = {*render_complete_semaphore};
	auto         command_buffers   = {*command_buffer};
	VkSubmitInfo main_queue_submit_info{
		.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount   = (uint32_t)wait_semaphores.size(),
		.pWaitSemaphores      = wait_semaphores.begin(),
		.pWaitDstStageMask    = (VkPipelineStageFlags*)wait_stage_mask.begin(),
		.commandBufferCount   = 1,
		.pCommandBuffers      = command_buffers.begin(),
		.signalSemaphoreCount = (uint32_t)signal_semaphores.size(),
		.pSignalSemaphores    = signal_semaphores.begin(),
	};

	check_success(
		vkQueueSubmit(graphics_queue, 1, &main_queue_submit_info, *next_frame_fence) == VK_SUCCESS,
		"Failed to submit queue"
	);

	// Present the image
	auto             present_wait_semaphore = {*render_complete_semaphore};
	auto             present_swapchains     = {*swapchain};
	VkPresentInfoKHR present_info{
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = (uint32_t)present_wait_semaphore.size(),
		.pWaitSemaphores    = present_wait_semaphore.begin(),
		.swapchainCount     = (uint32_t)present_swapchains.size(),
		.pSwapchains        = present_swapchains.begin(),
		.pImageIndices      = &next_image_index
	};

	vkQueuePresentKHR(presentation_queue, &present_info);
}
