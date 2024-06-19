#include "core.hpp"

bool Core::render_one_frame(const loop_func& loop_func)
{
	// Fetch next image
	uint32_t image_idx;

	while (true)
	{
		try
		{
			auto acquire_result = env.device->acquireNextImageKHR(env.swapchain.swapchain, 1e10, render_targets.acquire_semaphore);

			if (acquire_result.result == vk::Result::eSuccess) [[likely]]
			{
				image_idx = acquire_result.value;
				break;
			}

			if (acquire_result.result == vk::Result::eNotReady) continue;
		}
		catch (const vk::OutOfDateKHRError& e)
		{
		}

		recreate_swapchain();
	}

	const auto wait_fence_result = env.device->waitForFences({render_targets.next_frame_fence}, true, 1e10);
	if (wait_fence_result != vk::Result::eSuccess) throw Exception("Fence wait timeout!");
	env.device->resetFences({render_targets.next_frame_fence});

	// Record Command Buffer
	loop_func(image_idx);

	vk::Semaphore graphic_wait_semaphore = render_targets.acquire_semaphore, graphics_signal_semaphore = render_targets.render_done_semaphore,
				  present_wait_semaphore = graphics_signal_semaphore;

	// Submit to Graphic Queue
	{
		const auto wait_stages         = std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
		const auto command_buffer      = env.command_buffer[image_idx].to<vk::CommandBuffer>();

		env.g_queue.submit(
			vk::SubmitInfo()
				.setCommandBuffers(command_buffer)
				.setWaitSemaphores(graphic_wait_semaphore)
				.setSignalSemaphores(graphics_signal_semaphore)
				.setWaitDstStageMask(wait_stages),
			render_targets.next_frame_fence
		);
	}

	// Present
	{
		vk::SwapchainKHR target_swapchain = env.swapchain.swapchain;
		try
		{
			auto result = env.p_queue.presentKHR(
				vk::PresentInfoKHR().setWaitSemaphores(present_wait_semaphore).setSwapchains(target_swapchain).setImageIndices(image_idx)
			);
			if (result != vk::Result::eSuccess)
			{
				// wait_fence();
				// recreate_swapchain();
				std::cout << std::format("Present Result Abnormal: {}", (int)result) << '\n';
			}
		}
		catch (const vk::SystemError& err)
		{
			// wait_fence();
			// recreate_swapchain();
		}
	}

	return true;
}

void Core::recreate_swapchain()
{
	while (true)
	{
		SDL_Event event;
		if (SDL_PollEvent(&event))
		{
			auto flags = SDL_GetWindowFlags(env.window);

			int width, height;
			SDL_GetWindowSizeInPixels(env.window, &width, &height);

			if ((flags & SDL_WINDOW_MINIMIZED) == 0 && width > 0 && height > 0) break;
		}
	}

	env.device->waitIdle();

	for (auto i : Range(env.swapchain.image_count)) env.command_buffer[i].reset();

	env.swapchain  = Environment::Env_swapchain();
	render_targets = Render_targets();

	env.swapchain.create(env);
	render_targets.create(env, Pipeline_set);
	ui_controller.create_imgui_rt(env);
}