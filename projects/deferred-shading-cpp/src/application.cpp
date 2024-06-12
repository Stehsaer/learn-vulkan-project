#include "application.hpp"
#include "binary-resource.hpp"
#include <csignal>
#include <filesystem>
#include <thread>

void sig_handler_fpe(int sig_num)
{
	std::cerr << "[Error Report] Arithmatics Error! code=" << sig_num << '\n';
	exit(-1);
}

void sig_handler_sigsev(int sig_num)
{
	std::cerr << "[Error Report] Segment Fault! code=" << sig_num << '\n';
	exit(-2);
}

const char* cut_filename(const char* filename)
{
	while (true)
	{
		const char* const find1 = std::strstr(filename, "src/");
		const char* const find2 = std::strstr(filename, "src\\");

		if (find1 == nullptr && find2 == nullptr) return filename;

		if (find1 != nullptr) filename = find1 + 4;

		if (find2 != nullptr) filename = find2 + 4;
	}
}

int main(int argc [[maybe_unused]], char** argv [[maybe_unused]])
{
	// std::signal(SIGSEGV, sig_handler_sigsev);
	// std::signal(SIGFPE, sig_handler_fpe);

	std::shared_ptr<App_resource> shared_resource;

	auto show_error_msgbox
		= [](const std::string& title, const std::string& content, const std::shared_ptr<App_resource>& shared_resource)
	{
		if (shared_resource)
			if (shared_resource->env.window.is_valid())
			{
				SDL_ShowSimpleMessageBox(
					SDL_MESSAGEBOX_ERROR,
					title.c_str(),
					content.c_str(),
					shared_resource->env.window
				);
			}
	};

	try
	{
		shared_resource = std::make_shared<App_resource>();

		std::shared_ptr<Application_logic_base> current_logic = std::make_shared<App_idle_logic>(shared_resource);
		while (current_logic)
		{
			current_logic = current_logic->work();
		}

		terminate_sdl();

		shared_resource = nullptr;

		return EXIT_SUCCESS;
	}
	catch (const General_exception& err)
	{
		std::cerr << "General Error: " << err.what() << '\n';

		show_error_msgbox(
			"General Error",
			std::format(
				"General Error caught:\n[Line {} at {}]\n{}",
				err.loc.line(),
				cut_filename(err.loc.file_name()),
				err.msg
			),
			shared_resource
		);
	}
	catch (const vk::IncompatibleDriverError& err)
	{
		std::cerr << "Incompatible Driver!" << '\n';

		show_error_msgbox("Incompatible Driver", std::format("Incompatible Driver:\n{}", err.what()), shared_resource);
	}
	catch (const vk::DeviceLostError& err)
	{
		std::cerr << "Device Lost!" << '\n';

		show_error_msgbox("Device Lost", std::format("Vulkan Device Lost:\n{}", err.what()), shared_resource);
	}
	catch (const vk::SystemError& err)
	{
		std::cerr << "Vulkan Error: " << err.what() << '\n';

		show_error_msgbox("Vulkan Error", std::format("Vulkan Error caught:\n{}", err.what()), shared_resource);
	}
	catch (const std::exception& err)
	{
		std::cerr << "Unknown Error: " << err.what() << '\n';

		show_error_msgbox("Unknown Error", std::format("Unknown caught:\n{}", err.what()), shared_resource);
	}

#if NDEBUG
	system("pause");
#endif

	return EXIT_FAILURE;
}

bool App_resource::render_one_frame(const loop_func& loop_func)
{
	// Fetch next image
	uint32_t image_idx;

	while (true)
	{
		try
		{
			auto acquire_result
				= env.device->acquireNextImageKHR(swapchain.swapchain, 1e10, main_rt.acquire_semaphore);

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

	const auto wait_fence_result = env.device->waitForFences({main_rt.next_frame_fence}, true, 1e10);
	if (wait_fence_result != vk::Result::eSuccess) throw General_exception("Fence wait timeout!");
	env.device->resetFences({main_rt.next_frame_fence});

	// Record Command Buffer
	loop_func(image_idx);

	vk::Semaphore graphic_wait_semaphore    = main_rt.acquire_semaphore,
				  graphics_signal_semaphore = main_rt.render_done_semaphore,
				  present_wait_semaphore    = graphics_signal_semaphore;

	// Submit to Graphic Queue
	{
		const auto wait_stages
			= std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
		const auto compute_wait_stages
			= std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eComputeShader});
		const auto command_buffer = env.command_buffer[image_idx].to<vk::CommandBuffer>();

		if (env.c_queue != env.g_queue)
			env.c_queue.submit(
				vk::SubmitInfo().setCommandBuffers(command_buffer).setWaitDstStageMask(compute_wait_stages),
				{}
			);

		env.g_queue.submit(
			vk::SubmitInfo()
				.setCommandBuffers(command_buffer)
				.setWaitSemaphores(graphic_wait_semaphore)
				.setSignalSemaphores(graphics_signal_semaphore)
				.setWaitDstStageMask(wait_stages),
			main_rt.next_frame_fence
		);
	}

	// Present
	{
		vk::SwapchainKHR target_swapchain = swapchain.swapchain;
		try
		{
			auto result = env.p_queue.presentKHR(vk::PresentInfoKHR()
													 .setWaitSemaphores(present_wait_semaphore)
													 .setSwapchains(target_swapchain)
													 .setImageIndices(image_idx));
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

void App_resource::recreate_swapchain()
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

	for (auto i : Range(swapchain.image_count)) env.command_buffer[i].reset();

	swapchain = App_swapchain();
	main_rt   = Main_rt();

	swapchain.create(env);
	main_rt.create(env, swapchain, main_pipeline);
	ui_controller.create_imgui_rt(env, swapchain);
}