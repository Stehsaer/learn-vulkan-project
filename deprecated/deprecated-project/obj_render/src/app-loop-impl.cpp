#include "application.hpp"

void Obj_app::record_command_buffer(uint32_t swapchain_index)
{
	// Update descriptors before recording
	update_descriptors(swapchain_index);

	// Begin
	renderer.command_buf.reset();
	renderer.command_buf.begin();

	// Bind render pass
	{
		VkClearValue color_attachment_clear_value;
		color_attachment_clear_value.color = {
			{0.0, 0.0, 0.0, 0.0}
		};
		VkClearValue depth_attachment_clear_value;
		depth_attachment_clear_value.depthStencil.depth = 1.0f;

		auto clear_values = std::to_array({color_attachment_clear_value, depth_attachment_clear_value});

		renderer.command_buf.cmd.begin_render_pass(
			renderer.pipeline.render_pass,
			renderer.pipeline.framebuffers[swapchain_index],
			{
				.offset = {0, 0},
				.extent = swapchain.extent
        },
			clear_values
		);
	}

	// Bind pipeline
	renderer.command_buf.cmd.bind_graphics_pipeline(renderer.pipeline.handle);

	// Bind descriptor sets
	{
		auto descriptor_sets = std::to_array({*renderer.pipeline.descriptor_sets[swapchain_index]});
		renderer.command_buf.cmd
			.bind_descriptor_sets(renderer.pipeline.layout, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, descriptor_sets);
	}

	// Bind index buffer
	renderer.command_buf.cmd.bind_index_buffer(renderer.index_buffer, 0, VK_INDEX_TYPE_UINT32);

	// Bind vertex buffer
	{
		auto bind_vertex_buf = std::to_array({renderer.vertex_buffer->buffer});
		auto device_offsets  = std::to_array<VkDeviceSize>({0});
		renderer.command_buf.cmd.bind_vertex_buffers(0, 1, bind_vertex_buf, device_offsets);
	}

	// Update dynamic state
	{
		VkViewport current_viewport{
			.x        = 0,
			.y        = 0,
			.width    = (float)swapchain.extent.width,
			.height   = (float)swapchain.extent.height,
			.minDepth = 0.0,
			.maxDepth = 1.0
		};
		tools::flip_viewport(current_viewport);

		VkRect2D current_scissor{
			.offset = {0, 0},
			.extent = swapchain.extent
		};

		auto viewports = std::to_array({current_viewport});
		auto scissors  = std::to_array({current_scissor});

		renderer.command_buf.cmd.set_viewport(viewports);
		renderer.command_buf.cmd.set_scissor(scissors);
	}

	// Draw
	renderer.command_buf.cmd.draw_indexed(0, renderer.index_buffer->allocation.info.size / sizeof(uint32_t), 0, 1, 0);

	// End render pass
	renderer.command_buf.cmd.end_render_pass();

	// Draw Imgui Data
	{
		VkClearValue color_attachment_clear_value;
		color_attachment_clear_value.color = {
			{0.0, 0.0, 0.0, 0.0}
		};

		renderer.command_buf.cmd.begin_render_pass(
			imgui_render.render_pass,
			imgui_render.framebuffers[swapchain_index],
			VkRect2D{
				{0, 0},
				swapchain.extent
        },
			{&color_attachment_clear_value, 1}
		);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *renderer.command_buf);

		renderer.command_buf.cmd.end_render_pass();
	}

	// End
	auto end_result = renderer.command_buf.end();
	if (end_result != VK_SUCCESS) throw std::runtime_error("End main loop command buffer failed");
}

void Obj_app::update_descriptors(uint32_t swapchain_index)
{
	// Create & transfer uniform object
	{
		Uniform_object uniform;

		uniform.model = glm::scale(glm::mat4(1.0), glm::vec3(scale));

		auto mat = glm::rotate(glm::mat4(1), yaw, glm::vec3(0, 1, 0));
		mat      = glm::rotate(mat, pitch, glm::vec3(1, 0, 0));

		glm::vec3 eye_pos = glm::vec3(mat * glm::vec4(0, 0, 1, 1));

		uniform.view       = glm::lookAt(eye_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
		uniform.projection = glm::perspective(
			glm::radians(45.0f),
			(float)swapchain.extent.width / swapchain.extent.height,
			0.1f,
			10.0f
		);

		renderer.uniform_buffers[swapchain_index]->allocation.transfer(uniform);
	}

	// Uniform info
	VkDescriptorBufferInfo uniform_info{
		.buffer = renderer.uniform_buffers[swapchain_index]->buffer,
		.offset = 0,
		.range  = sizeof(Uniform_object)
	};

	VkWriteDescriptorSet uniform_write{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = *renderer.pipeline.descriptor_sets[swapchain_index],
		.dstBinding      = 0,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo     = &uniform_info
	};

	// Bind sampler to descriptor set
	VkDescriptorImageInfo texture_info{
		.sampler     = *renderer.texture_sampler,
		.imageView   = *renderer.texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet texture_write{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = *renderer.pipeline.descriptor_sets[swapchain_index],
		.dstBinding      = 1,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo      = &texture_info
	};

	// Write
	auto write_info_list = std::to_array({uniform_write, texture_write});
	renderer.pipeline.descriptor_sets[swapchain_index].update(write_info_list, {});
}

void Obj_app::recreate_swapchain()
{
	// Wait until not minimized
	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(*environment.window, &width, &height);
		glfwWaitEvents();
	}

	environment.device.wait_idle();

	// manually clean up framebuffers and swapchain image views
	for (auto& fb : renderer.pipeline.framebuffers) fb.destroy();
	for (auto& fb : imgui_render.framebuffers) fb.destroy();
	for (auto& swapchain_image_view : swapchain.image_views) swapchain_image_view.destroy();

	sync.idle_fence.reset();
	renderer.command_buf.reset();

	// recreate
	swapchain.init(environment);
	renderer.pipeline.init(environment, swapchain, true);
	imgui_render.init_graphics(environment, swapchain);
}

void Obj_app::draw()
{
	// Sync fence
	{
		auto result = sync.idle_fence.wait(1e9);
		if (result != VK_SUCCESS) throw std::runtime_error("Fence Synchronization Failed");
		sync.idle_fence.reset();
	}

	uint32_t swapchain_index;

	// acquire image
	while (true)
	{
		auto result
			= swapchain.handle.acquire_next_image(sync.swapchain_ready_semaphore, std::nullopt) >> swapchain_index;

		if (!result)
		{
			switch (result.err())
			{
			case VK_SUBOPTIMAL_KHR:
			case VK_ERROR_OUT_OF_DATE_KHR:
				recreate_swapchain();
				continue;
			default:
				throw std::runtime_error("Acquire Swapchain Failed");
			}
		}

		// acquirement success, break loop
		break;
	}

	// record buffers
	// renderer.command_buf.reset();
	record_command_buffer(swapchain_index);

	// Submit
	{
		auto submit_wait_semaphore   = {*sync.swapchain_ready_semaphore};
		auto submit_wait_stage_mask  = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		auto submit_signal_semaphore = {*sync.render_complete_semaphore};
		auto command_buffers         = {*renderer.command_buf};

		VkSubmitInfo draw_queue_submit_info{
			.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount   = (uint32_t)submit_wait_semaphore.size(),
			.pWaitSemaphores      = submit_wait_semaphore.begin(),
			.pWaitDstStageMask    = (VkPipelineStageFlags*)submit_wait_stage_mask.begin(),
			.commandBufferCount   = 1,
			.pCommandBuffers      = command_buffers.begin(),
			.signalSemaphoreCount = (uint32_t)submit_signal_semaphore.size(),
			.pSignalSemaphores    = submit_signal_semaphore.begin(),
		};

		auto result = vkQueueSubmit(environment.g_queue, 1, &draw_queue_submit_info, *sync.idle_fence);
		if (result != VK_SUCCESS) throw std::runtime_error("Submit Draws Failed");
	}

	// Present
	{
		auto             present_wait_semaphores = {*sync.render_complete_semaphore};
		auto             present_swapchains      = {*swapchain.handle};
		VkPresentInfoKHR present_info{
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = (uint32_t)present_wait_semaphores.size(),
			.pWaitSemaphores    = present_wait_semaphores.begin(),
			.swapchainCount     = (uint32_t)present_swapchains.size(),
			.pSwapchains        = present_swapchains.begin(),
			.pImageIndices      = &swapchain_index
		};

		vkQueuePresentKHR(environment.p_queue, &present_info);
	}
}

void Obj_app::imgui_logic()
{
	imgui_render.new_frame();
	ImGui::ShowDemoWindow();

	float framerate = ImGui::GetIO().Framerate;
	float dt        = ImGui::GetIO().DeltaTime;
	ImGui::SetNextWindowPos({20, (float)swapchain.extent.height - 20}, ImGuiCond_Always, {0, 1});
	ImGui::Begin(
		"Framerate",
		NULL,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize
	);
	{
		ImGui::Text("FPS: %.1f", framerate);
		ImGui::Text("DT: %.2fms", dt * 1000);
	}
	ImGui::End();

	if (!ImGui::GetIO().WantCaptureMouse
		&& (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::GetIO().MouseWheel != 0.0f))
	{
		auto& io = ImGui::GetIO();
		yaw -= io.MouseDelta.x * 0.003;
		pitch -= io.MouseDelta.y * 0.003;
		scale *= pow(1.1, io.MouseWheel);

		yaw   = glm::mod(yaw, 2 * glm::pi<float>());
		pitch = glm::clamp(pitch, -glm::pi<float>() / 2 + 0.0001f, glm::pi<float>() / 2 - 0.0001f);
		scale = glm::clamp(scale, 0.00001f, 100000.0f);
	}
	ImGui::Render();
}

void Obj_app::loop()
{
	while (!environment.window.should_close())
	{
		glfwPollEvents();
		imgui_logic();
		draw();
	}

	environment.device.wait_idle();
	imgui_render.destroy();
}
