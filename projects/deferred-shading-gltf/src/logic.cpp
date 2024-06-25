#include "logic.hpp"
#include "binary-resource.hpp"
#include <filesystem>

#define LOAD_DEFAULT_MODEL_TOKEN "**cmd-load-built-in--model**"
#define LOAD_DEFUALT_HDRI_TOKEN "**cmd-load-built-in--hdri**"

#pragma region "App Idle Logic"

std::shared_ptr<Application_logic_base> App_idle_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);

	while (true)
	{
		// Poll SDL Events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;

			// File Dropped
			if (event.type == SDL_DROPFILE)
			{
				const auto file_path_str = std::string(event.drop.file);
				const auto path          = std::filesystem::path(file_path_str);
				const auto extension     = path.extension();

				if (!std::filesystem::is_regular_file(path))
				{
					continue;
				}

				if (extension == ".gltf" || extension == ".glb") return std::make_shared<App_load_model_logic>(core, file_path_str);

				if (extension == ".hdr") return std::make_shared<App_load_hdri_logic>(core, file_path_str);
			}

			if (load_builtin_model) return std::make_shared<App_load_model_logic>(core, LOAD_DEFAULT_MODEL_TOKEN);

			if (load_builtin_hdri) return std::make_shared<App_load_hdri_logic>(core, LOAD_DEFUALT_HDRI_TOKEN);
		}

		core->ui_controller.imgui_new_frame();

		core->render_one_frame(
			[this](uint32_t idx)
			{
				draw(idx);
			}
		);
	}
}

void App_idle_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_idle_logic::ui_logic()
{
	ImGui::SetNextWindowPos(ImVec2(core->env.swapchain.extent.width / 2.0, core->env.swapchain.extent.height / 2.0), ImGuiCond_Always, {0.5, 0.5});
	if (ImGui::Begin(
			"##Load Window",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		))
	{
		ImGui::Text("Drop File Here to Load!");
		ImGui::Separator();

		ImGui::BulletText("Model: %s", core->params.model == nullptr ? "Not Loaded" : "Loaded");
		ImGui::BulletText("HDRI: %s", core->params.hdri == nullptr ? "Not Loaded" : "Loaded");

		ImGui::Separator();
		if (ImGui::Button("Load Built-in Model")) load_builtin_model = true;

		if (ImGui::Button("Load Built-in HDRI")) load_builtin_hdri = true;
	}
	ImGui::End();

	ImGui::Render();
}

#pragma endregion

#pragma region "App Render logic"

std::shared_ptr<Application_logic_base> App_render_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);

	while (true)
	{
		// Poll SDL Events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;

			// File Dropped
			if (event.type == SDL_DROPFILE)
			{
				const auto file_path_str = std::string(event.drop.file);
				const auto path          = std::filesystem::path(file_path_str);
				const auto extension     = path.extension();

				if (!std::filesystem::is_regular_file(path))
				{
					continue;
				}

				if (extension == ".gltf" || extension == ".glb") return std::make_shared<App_load_model_logic>(core, file_path_str);

				if (extension == ".hdr") return std::make_shared<App_load_hdri_logic>(core, file_path_str);
			}
		}

		core->ui_controller.imgui_new_frame();
		core->params.camera_controller.update(ImGui::GetIO());

		uint32_t image_idx;

		while (true)
		{
			try
			{
				auto acquire_result = core->env.device->acquireNextImageKHR(
					core->env.swapchain.swapchain,
					1e10,
					core->render_targets.acquire_semaphore
				);

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

			stop_threads();
			core->recreate_swapchain();
			start_threads();
		}

		const auto wait_fence_result = core->env.device->waitForFences({core->render_targets.next_frame_fence}, true, 1e10);
		if (wait_fence_result != vk::Result::eSuccess) throw Exception("Fence wait timeout!");
		core->env.device->resetFences({core->render_targets.next_frame_fence});

		// Record Command Buffer
		draw(image_idx);

		// Submit
		const auto deferred_signal_semaphore = Semaphore::to_array({deferred_semaphore});
		const auto deferred_submit_buffers = utility::join_array(
			Command_buffer::to_array({current_gbuffer_command_buffer}),
			Command_buffer::to_array(current_shadow_command_buffer)
		);

		const auto deferred_submit_info
			= vk::SubmitInfo().setCommandBuffers(deferred_submit_buffers).setSignalSemaphores(deferred_signal_semaphore);

		const auto lighting_signal_semaphore = Semaphore::to_array({lighting_semaphore});
		const auto lighting_wait_semaphore   = Semaphore::to_array({deferred_semaphore});
		const auto lighting_wait_stages = std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
		const auto lighting_submit_buffers = Command_buffer::to_array({current_lighting_command_buffer});
		const auto lighting_submit_info    = vk::SubmitInfo()
											  .setCommandBuffers(lighting_submit_buffers)
											  .setWaitDstStageMask(lighting_wait_stages)
											  .setWaitSemaphores(lighting_wait_semaphore)
											  .setSignalSemaphores(lighting_signal_semaphore);

		const auto compute_signal_semaphore = Semaphore::to_array({compute_semaphore});
		const auto compute_wait_semaphore   = Semaphore::to_array({lighting_semaphore});
		const auto compute_wait_stages    = std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
		const auto compute_submit_buffers = Command_buffer::to_array({current_compute_command_buffer});
		const auto compute_submit_info    = vk::SubmitInfo()
											 .setCommandBuffers(compute_submit_buffers)
											 .setWaitDstStageMask(compute_wait_stages)
											 .setWaitSemaphores(compute_wait_semaphore)
											 .setSignalSemaphores(compute_signal_semaphore);

		const auto composite_signal_semaphore = Semaphore::to_array({composite_semaphore});
		const auto composite_wait_semaphore   = Semaphore::to_array({compute_semaphore, core->render_targets.acquire_semaphore});
		const auto composite_wait_stages      = std::to_array<vk::PipelineStageFlags>(
            {vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput}
        );
		const auto composite_submit_buffers   = Command_buffer::to_array({current_composite_command_buffer});
		const auto composite_submit_info      = vk::SubmitInfo()
											   .setCommandBuffers(composite_submit_buffers)
											   .setWaitDstStageMask(composite_wait_stages)
											   .setWaitSemaphores(composite_wait_semaphore)
											   .setSignalSemaphores(composite_signal_semaphore);

		core->env.g_queue.submit({deferred_submit_info, lighting_submit_info});
		core->env.c_queue.submit({compute_submit_info});
		core->env.g_queue.submit({composite_submit_info}, core->render_targets.next_frame_fence);

		// Present
		{
			vk::SwapchainKHR target_swapchain       = core->env.swapchain.swapchain;
			const auto       present_wait_semaphore = Semaphore::to_array({composite_semaphore});
			try
			{
				auto result = core->env.p_queue.presentKHR(vk::PresentInfoKHR()
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
	}
}

void App_render_logic::start_threads()
{
	for (auto i : Range(csm_count))
		shadow_thread[i] = std::jthread(
			[this, i]()
			{
				shadow_thread_work(i);
			}
		);

	gbuffer_thread = std::jthread(
		[this]()
		{
			gbuffer_thread_work();
		}
	);

	post_thread = std::jthread(
		[this]()
		{
			post_thread_work();
		}
	);
}

void App_render_logic::stop_threads()
{
	multi_thread_stop = true;
	frame_start_semaphore.release(csm_count + 2);

	for (auto& thread : shadow_thread) thread.join();
	gbuffer_thread.join();
	post_thread.join();

	multi_thread_stop = false;
}

void App_render_logic::draw(uint32_t idx)
{
	// update draw parameters
	{
		draw_params = core->params.get_draw_parameters(core->env);

		const Gbuffer_pipeline::Camera_uniform gbuffer_camera{draw_params.view_projection};

		Lighting_pipeline::Params lighting_params{
			glm::inverse(draw_params.view_projection),
			draw_params.eye_position,
			{},
			draw_params.light_dir,
			core->params.light_color * core->params.light_intensity,
			core->params.emissive_brightness,
			core->params.skybox_brightness
		};

		std::array<Shadow_pipeline::Shadow_uniform, csm_count> shadow_uniforms;

		for (auto i : Range(csm_count))
		{
			lighting_params.shadow[i] = draw_params.shadow_transformations[i];
			shadow_uniforms[i]        = {draw_params.shadow_transformations[i]};
		}

		const Composite_pipeline::Exposure_param composite_param{exp2(core->params.exposure_ev), core->params.bloom_intensity};

		core->render_targets[idx].update(core->env, shadow_uniforms, gbuffer_camera, lighting_params, composite_param);

		// Setup variables
		current_idx          = idx;
		gbuffer_cpu_time     = 0;
		gbuffer_object_count = 0;
		gbuffer_vertex_count = 0;
		shadow_cpu_time      = 0;
		shadow_object_count  = 0;
		shadow_vertex_count  = 0;
	}

	// Triggers sub-threads
	frame_start_semaphore.release(csm_count + 2);

	// Synchronize multiple threads
	render_thread_barrier.arrive_and_wait();
}

void App_render_logic::shadow_thread_work(uint32_t csm_idx)
{
	const Command_pool local_command_pool(
		core->env.device,
		core->env.g_family_idx,
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer
	);
	const std::vector<Command_buffer> command_buffers
		= Command_buffer::allocate_multiple_from(local_command_pool, core->env.swapchain.image_count);

	// draw loop
	while (true)
	{
		frame_start_semaphore.acquire();
		if (multi_thread_stop) return;

		const auto& command_buffer = command_buffers[current_idx];

		command_buffer.begin();

		core->env.debug_marker.begin_region(command_buffer, std::format("Render Shadow Map, Level {}", csm_idx), {1.0, 1.0, 0.0, 1.0});

		// Begin Shadow Renderpass
		command_buffer.begin_render_pass(
			core->Pipeline_set.shadow_pipeline.render_pass,
			core->render_targets[current_idx].shadow_rt.shadow_framebuffers[csm_idx],
			vk::Rect2D({0, 0}, {shadow_map_res[csm_idx], shadow_map_res[csm_idx]}),
			{Shadow_pipeline::clear_value},
			vk::SubpassContents::eInline
		);

		// Set Viewport and Scissors
		command_buffer.set_viewport(utility::flip_viewport(vk::Viewport(0, 0, shadow_map_res[csm_idx], shadow_map_res[csm_idx], 0.0, 1.0)));

		command_buffer.set_scissor({
			{0,					   0					  },
			{shadow_map_res[csm_idx], shadow_map_res[csm_idx]}
		});

		// Bind Matrices
		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->Pipeline_set.shadow_pipeline.pipeline_layout,
			0,
			{core->render_targets[current_idx].shadow_rt.shadow_matrix_descriptor_set[csm_idx]}
		);

		auto bind_descriptor = [=, this](auto material)
		{
			command_buffer.bind_descriptor_sets(
				vk::PipelineBindPoint::eGraphics,
				core->Pipeline_set.shadow_pipeline.pipeline_layout,
				1,
				{material.albedo_only_descriptor_set}
			);
		};

		auto bind_vertex = [=, this](const io::mesh::gltf::Primitive& primitive)
		{
			const auto& model = core->params.model;
			command_buffer->bindVertexBuffers(
				0,
				{model->vec3_buffers[primitive.position_buffer], model->vec2_buffers[primitive.uv_buffer]},
				{primitive.position_offset * sizeof(glm::vec3), primitive.uv_offset * sizeof(glm::vec2)}
			);
		};

		auto bind_vertex_opaque = [=, this](const io::mesh::gltf::Primitive& primitive)
		{
			const auto& model = core->params.model;
			command_buffer->bindVertexBuffers(
				0,
				{model->vec3_buffers[primitive.position_buffer]},
				{primitive.position_offset * sizeof(glm::vec3)}
			);
		};

		auto draw_result = shadow_renderer[csm_idx].render_gltf(
			command_buffer,
			*core->params.model,
			draw_params.shadow_frustums[csm_idx],
			{0, 0, 0},
			-draw_params.light_dir,
			core->Pipeline_set.shadow_pipeline.single_sided_pipeline,
			core->Pipeline_set.shadow_pipeline.single_sided_pipeline_alpha,
			core->Pipeline_set.shadow_pipeline.single_sided_pipeline_blend,
			core->Pipeline_set.shadow_pipeline.double_sided_pipeline,
			core->Pipeline_set.shadow_pipeline.double_sided_pipeline_alpha,
			core->Pipeline_set.shadow_pipeline.double_sided_pipeline_blend,
			core->Pipeline_set.shadow_pipeline.pipeline_layout,
			bind_descriptor,
			bind_vertex_opaque,
			bind_vertex,
			bind_vertex,
			true
		);

		command_buffer.end_render_pass();

		core->env.debug_marker.end_region(command_buffer);

		command_buffer.end();

		draw_result.near = std::min((draw_result.near + draw_result.far) / 2.0f - 0.01f, draw_result.near);
		draw_result.far  = std::max((draw_result.near + draw_result.far) / 2.0f + 0.01f, draw_result.far);

		core->params.shadow_far[csm_idx]  = draw_result.far;
		core->params.shadow_near[csm_idx] = draw_result.near;

		shadow_cpu_time += draw_result.time_consumed;
		shadow_object_count += shadow_renderer[csm_idx].get_object_count();
		shadow_vertex_count += draw_result.vertex_count;

		current_shadow_command_buffer[csm_idx] = command_buffer;

		model_rendering_statistic_barrier.arrive_and_wait();

		render_thread_barrier.arrive_and_wait();
	}
}

void App_render_logic::gbuffer_thread_work()
{
	const Command_pool local_command_pool(
		core->env.device,
		core->env.g_family_idx,
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer
	);
	const std::vector<Command_buffer> command_buffers
		= Command_buffer::allocate_multiple_from(local_command_pool, core->env.swapchain.image_count);

	const auto draw_extent = vk::Rect2D({0, 0}, core->env.swapchain.extent);

	// draw loop
	while (true)
	{
		frame_start_semaphore.acquire();

		if (multi_thread_stop) return;

		const auto& command_buffer = command_buffers[current_idx];

		command_buffer.begin();

		core->env.debug_marker.begin_region(command_buffer, "Render Gbuffer", {0.0, 1.0, 1.0, 1.0});

		command_buffer.begin_render_pass(
			core->Pipeline_set.gbuffer_pipeline.render_pass,
			core->render_targets[current_idx].gbuffer_rt.framebuffer,
			draw_extent,
			Gbuffer_pipeline::clear_values
		);
		{
			command_buffer.set_viewport(utility::flip_viewport(
				vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0)
			));
			command_buffer.set_scissor(vk::Rect2D({0, 0}, core->env.swapchain.extent));

			command_buffer.bind_descriptor_sets(
				vk::PipelineBindPoint::eGraphics,
				core->Pipeline_set.gbuffer_pipeline.pipeline_layout,
				0,
				{core->render_targets[current_idx].gbuffer_rt.camera_uniform_descriptor_set}
			);

			auto bind_descriptor = [&](auto material)
			{
				command_buffer.bind_descriptor_sets(
					vk::PipelineBindPoint::eGraphics,
					core->Pipeline_set.gbuffer_pipeline.pipeline_layout,
					1,
					{material.descriptor_set}
				);
			};

			auto bind_vertex = [=, this](const io::mesh::gltf::Primitive& primitive)
			{
				const auto& model = core->params.model;
				command_buffer->bindVertexBuffers(
					0,
					{model->vec3_buffers[primitive.position_buffer],
					 model->vec3_buffers[primitive.normal_buffer],
					 model->vec2_buffers[primitive.uv_buffer],
					 model->vec3_buffers[primitive.tangent_buffer]},
					{primitive.position_offset * sizeof(glm::vec3),
					 primitive.normal_offset * sizeof(glm::vec3),
					 primitive.uv_offset * sizeof(glm::vec2),
					 primitive.tangent_offset * sizeof(glm::vec3)}
				);
			};

			auto draw_result = gbuffer_renderer.render_gltf(
				command_buffer,
				*(core->params.model),
				draw_params.gbuffer_frustum,
				draw_params.eye_position,
				draw_params.eye_path,
				core->Pipeline_set.gbuffer_pipeline.single_sided_pipeline,
				core->Pipeline_set.gbuffer_pipeline.single_sided_pipeline_alpha,
				core->Pipeline_set.gbuffer_pipeline.single_sided_pipeline_blend,
				core->Pipeline_set.gbuffer_pipeline.double_sided_pipeline,
				core->Pipeline_set.gbuffer_pipeline.double_sided_pipeline_alpha,
				core->Pipeline_set.gbuffer_pipeline.double_sided_pipeline_blend,
				core->Pipeline_set.gbuffer_pipeline.pipeline_layout,
				bind_descriptor,
				bind_vertex,
				bind_vertex,
				bind_vertex,
				true
			);

			draw_result.far  = std::max(0.02f, draw_result.far);
			draw_result.near = std::max(0.01f, draw_result.near);
			draw_result.near = std::min(draw_result.near, draw_result.far - 0.01f);
			draw_result.near = std::max(draw_result.near, draw_result.far / 200);

			if (core->params.auto_adjust_near_plane) core->params.near = draw_result.near;
			if (core->params.auto_adjust_far_plane) core->params.far = draw_result.far;

			gbuffer_object_count = gbuffer_renderer.get_object_count();
			gbuffer_cpu_time = draw_result.time_consumed;
			gbuffer_vertex_count = draw_result.vertex_count;
		}
		command_buffer.end_render_pass();

		core->env.debug_marker.end_region(command_buffer);

		command_buffer.end();

		current_gbuffer_command_buffer = command_buffer;

		model_rendering_statistic_barrier.arrive_and_wait();

		render_thread_barrier.arrive_and_wait();
	}
}

void App_render_logic::post_thread_work()
{
	const Command_pool local_command_pool(
		core->env.device,
		core->env.g_family_idx,
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer
	),
		local_compute_command_pool(core->env.device, core->env.c_family_idx, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	const std::vector<Command_buffer> lighting_command_buffer
		= Command_buffer::allocate_multiple_from(local_command_pool, core->env.swapchain.image_count),
		compute_command_buffer   = Command_buffer::allocate_multiple_from(local_compute_command_pool, core->env.swapchain.image_count),
		composite_command_buffer = Command_buffer::allocate_multiple_from(local_command_pool, core->env.swapchain.image_count);

	// draw loop
	while (true)
	{
		frame_start_semaphore.acquire();

		if (multi_thread_stop) return;

		lighting_command_buffer[current_idx].begin();
		draw_lighting(current_idx, lighting_command_buffer[current_idx]);
		lighting_command_buffer[current_idx].end();

		compute_command_buffer[current_idx].begin();
		compute_auto_exposure(current_idx, compute_command_buffer[current_idx]);
		compute_bloom(current_idx, compute_command_buffer[current_idx]);
		compute_command_buffer[current_idx].end();

		model_rendering_statistic_barrier.arrive_and_wait();

		composite_command_buffer[current_idx].begin();
		{
			// Draw Composite
			draw_composite(current_idx, composite_command_buffer[current_idx]);

			// Draw UI
			ui_logic();
			core->env.debug_marker.begin_region(composite_command_buffer[current_idx], "Draw IMGUI", {0.7, 0.0, 1.0, 1.0});
			core->ui_controller.imgui_draw(core->env, composite_command_buffer[current_idx], current_idx, false);
			core->env.debug_marker.end_region(composite_command_buffer[current_idx]);
		}
		composite_command_buffer[current_idx].end();

		current_lighting_command_buffer  = lighting_command_buffer[current_idx];
		current_compute_command_buffer   = compute_command_buffer[current_idx];
		current_composite_command_buffer = composite_command_buffer[current_idx];

		render_thread_barrier.arrive_and_wait();
	}
}

void App_render_logic::draw_lighting(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto draw_extent = vk::Rect2D({0, 0}, core->env.swapchain.extent);

	auto set_viewport = [=, this](bool flip)
	{
		if (flip)
			command_buffer.set_viewport(utility::flip_viewport(
				vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0)
			));
		else
			command_buffer.set_viewport(vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0));

		command_buffer.set_scissor(vk::Rect2D({0, 0}, core->env.swapchain.extent));
	};

	// Lighting Pass
	core->env.debug_marker.begin_region(command_buffer, "Render Lighting", {0.0, 0.0, 1.0, 1.0});
	command_buffer.begin_render_pass(
		core->Pipeline_set.lighting_pipeline.render_pass,
		core->render_targets[idx].lighting_rt.framebuffer,
		draw_extent,
		{Lighting_pipeline::clear_value}
	);
	{
		set_viewport(false);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->Pipeline_set.lighting_pipeline.pipeline_layout,
			0,
			{core->render_targets[idx].lighting_rt.input_descriptor_set, core->params.hdri->descriptor_set}
		);

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, core->Pipeline_set.lighting_pipeline.pipeline);

		command_buffer.draw(0, 4, 0, 1);
	}
	command_buffer.end_render_pass();
	core->env.debug_marker.end_region(command_buffer);
}

void App_render_logic::compute_auto_exposure(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto  g_queue_family = core->env.g_family_idx, c_queue_family = core->env.c_family_idx;

	core->env.debug_marker.begin_region(command_buffer, "Compute Auto Exposure", {1.0, 0.0, 0.0, 1.0});
	{  // Sync 1
		const vk::ImageMemoryBarrier barrier(
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eGeneral,
			g_queue_family,
			c_queue_family,
			core->render_targets[idx].lighting_rt.brightness,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		const vk::BufferMemoryBarrier buffer_barrier(
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderWrite,
			c_queue_family,
			c_queue_family,
			core->render_targets.auto_exposure_rt.medium_buffer,
			0,
			sizeof(Auto_exposure_compute_pipeline::Exposure_medium)
		);

		command_buffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlagBits::eByRegion,
			{},
			{},
			{barrier}
		);

		command_buffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlagBits::eByRegion,
			{},
			{buffer_barrier},
			{}
		);
	}
	{  // Dispatch Avg Calculation
		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, core->Pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eCompute,
			core->Pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline_layout,
			0,
			{core->render_targets.auto_exposure_rt.luminance_avg_descriptor_sets[idx]}
		);

		const Auto_exposure_compute_pipeline::Luminance_params params{
			Auto_exposure_compute_pipeline::min_luminance,
			Auto_exposure_compute_pipeline::max_luminance
		};

		command_buffer.push_constants(core->Pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline_layout, vk::ShaderStageFlagBits::eCompute, params, 0);

		const auto group_size = Auto_exposure_compute_pipeline::luminance_avg_workgroup_size;
		command_buffer->dispatch(
			(uint32_t)ceil((float)core->env.swapchain.extent.width / group_size.x),
			(uint32_t)ceil((float)core->env.swapchain.extent.height / group_size.y),
			1
		);
	}
	{  // Sync 2
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			c_queue_family,
			c_queue_family,
			core->render_targets.auto_exposure_rt.medium_buffer,
			0,
			sizeof(Auto_exposure_compute_pipeline::Exposure_medium)
		);

		command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {barrier}, {});
	}
	{  // Dispatch Exposure Lerp
		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, core->Pipeline_set.auto_exposure_pipeline.lerp_pipeline);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eCompute,
			core->Pipeline_set.auto_exposure_pipeline.lerp_pipeline_layout,
			0,
			{core->render_targets.auto_exposure_rt.lerp_descriptor_set}
		);

		Auto_exposure_compute_pipeline::Lerp_params params;
		params.adapt_speed    = core->params.adapt_speed;
		params.delta_time     = ImGui::GetIO().DeltaTime;
		params.min_luminance  = Auto_exposure_compute_pipeline::min_luminance;
		params.max_luminance  = Auto_exposure_compute_pipeline::max_luminance;
		params.texture_size_x = core->env.swapchain.extent.width;
		params.texture_size_y = core->env.swapchain.extent.height;

		command_buffer.push_constants(core->Pipeline_set.auto_exposure_pipeline.lerp_pipeline_layout, vk::ShaderStageFlagBits::eCompute, params, 0);

		command_buffer->dispatch(1, 1, 1);
	}
	{  // Sync 3
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead,
			c_queue_family,
			g_queue_family,
			core->render_targets.auto_exposure_rt.out_buffer,
			0,
			sizeof(Auto_exposure_compute_pipeline::Exposure_result)
		);

		command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {barrier}, {});
	}
	core->env.debug_marker.end_region(command_buffer);
}

void App_render_logic::compute_bloom(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto  g_queue_family = core->env.g_family_idx, c_queue_family = core->env.c_family_idx;

	const auto& rt        = core->render_targets[idx];
	const auto& pipeline  = core->Pipeline_set;
	const auto& swapchain = core->env.swapchain;

	core->env.debug_marker.begin_region(command_buffer, "Compute Bloom", {1.0, 0.0, 0.0, 1.0});
	{  // Sync
		command_buffer.layout_transit(
			rt.lighting_rt.luminance,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eGeneral,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eComputeShader,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
			vk::DependencyFlagBits::eByRegion,
			g_queue_family,
			c_queue_family
		);

		command_buffer.layout_transit(
			rt.bloom_rt.bloom_downsample_chain,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eTransferRead,
			vk::AccessFlagBits::eShaderWrite,
			vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			{vk::ImageAspectFlagBits::eColor, 0, bloom_downsample_count, 0, 1},
			vk::DependencyFlagBits::eByRegion
		);

		command_buffer.layout_transit(
			rt.bloom_rt.bloom_upsample_chain,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
			vk::AccessFlagBits::eShaderRead,
			vk::AccessFlagBits::eShaderWrite,
			vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eComputeShader,
			{vk::ImageAspectFlagBits::eColor, 0, bloom_downsample_count - 2, 0, 1},
			vk::DependencyFlagBits::eByRegion
		);

		const vk::BufferMemoryBarrier exposure_barrier{
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::QueueFamilyIgnored,
			vk::QueueFamilyIgnored,
			core->render_targets.auto_exposure_rt.out_buffer,
			0,
			sizeof(Auto_exposure_compute_pipeline::Exposure_result)
		};

		command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {exposure_barrier}, {});
	}

	// Filter
	core->env.debug_marker.begin_region(command_buffer, "Filter Pixels", {1.0, 0.5, 0.0, 1.0});
	{
		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, pipeline.bloom_pipeline.bloom_filter_pipeline);
		command_buffer->bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			pipeline.bloom_pipeline.bloom_filter_pipeline_layout,
			0,
			{rt.bloom_rt.bloom_filter_descriptor_set},
			{}
		);

		const Bloom_pipeline::Params params{core->params.bloom_start, core->params.bloom_end, exp2(core->params.exposure_ev)};

		command_buffer.push_constants(pipeline.bloom_pipeline.bloom_filter_pipeline_layout, vk::ShaderStageFlagBits::eCompute, params, 0);

		// Filter pixels
		command_buffer->dispatch(ceil((float)swapchain.extent.width / 16), ceil((float)swapchain.extent.height / 16), 1);
	}
	core->env.debug_marker.end_region(command_buffer);

	// Downsample & Blur
	{
		const auto downsample_image = rt.bloom_rt.bloom_downsample_chain;
		const auto upsample_image   = rt.bloom_rt.bloom_upsample_chain;

		auto blit_image = [=](uint32_t i)
		{
			vk::ImageBlit blit_info;
			const auto    extent1 = rt.bloom_rt.extents[i];
			const auto    extent2 = rt.bloom_rt.extents[i + 1];

			std::array<vk::Offset3D, 2> src_region, dst_region;

			src_region[0] = vk::Offset3D{0, 0, 0};
			src_region[1] = vk::Offset3D{(int)extent1.width, (int)extent1.height, 1};
			dst_region[0] = vk::Offset3D{0, 0, 0};
			dst_region[1] = vk::Offset3D{(int)extent2.width, (int)extent2.height, 1};

			blit_info.setSrcSubresource({vk::ImageAspectFlagBits::eColor, i - 1, 0, 1})
				.setDstSubresource({vk::ImageAspectFlagBits::eColor, i + 1, 0, 1})
				.setSrcOffsets(src_region)
				.setDstOffsets(dst_region);

			// Do blit
			command_buffer.blit_image(upsample_image, vk::ImageLayout::eGeneral, downsample_image, vk::ImageLayout::eGeneral, {blit_info}, vk::Filter::eLinear);
		};

		auto blit_filter = [=]
		{
			vk::ImageBlit blit_info;
			const auto    extent1 = rt.bloom_rt.extents[0];
			const auto    extent2 = rt.bloom_rt.extents[1];

			std::array<vk::Offset3D, 2> src_region, dst_region;

			src_region[0] = vk::Offset3D{0, 0, 0};
			src_region[1] = vk::Offset3D{(int)extent1.width, (int)extent1.height, 1};
			dst_region[0] = vk::Offset3D{0, 0, 0};
			dst_region[1] = vk::Offset3D{(int)extent2.width, (int)extent2.height, 1};

			blit_info.setSrcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
				.setDstSubresource({vk::ImageAspectFlagBits::eColor, 1, 0, 1})
				.setSrcOffsets(src_region)
				.setDstOffsets(dst_region);

			// Do blit
			command_buffer
				.blit_image(downsample_image, vk::ImageLayout::eGeneral, downsample_image, vk::ImageLayout::eGeneral, {blit_info}, vk::Filter::eLinear);
		};

		// downsample
		core->env.debug_marker.begin_region(command_buffer, "Downsample", {1.0, 1.0, 0.0, 1.0});
		command_buffer.layout_transit(
			upsample_image,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eGeneral,
			vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eTransferRead,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eTransfer,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);
		blit_filter();

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, pipeline.bloom_pipeline.bloom_blur_pipeline);
		for (uint32_t i = 1; i < bloom_downsample_count - 1; i++)
		{
			command_buffer->bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				pipeline.bloom_pipeline.bloom_blur_pipeline_layout,
				0,
				{rt.bloom_rt.bloom_blur_descriptor_sets[i - 1]},
				{}
			);

			command_buffer.layout_transit(
				downsample_image,
				vk::ImageLayout::eGeneral,
				vk::ImageLayout::eGeneral,
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eShaderRead,
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader,
				{vk::ImageAspectFlagBits::eColor, i, 1, 0, 1}
			);

			const auto extent = rt.bloom_rt.extents[i];

			command_buffer->dispatch(ceil((float)extent.width / 16), ceil((float)extent.height / 16), 1);

			// Sync blur to transfer
			command_buffer.layout_transit(
				upsample_image,
				vk::ImageLayout::eGeneral,
				vk::ImageLayout::eGeneral,
				vk::AccessFlagBits::eShaderWrite,
				vk::AccessFlagBits::eTransferRead,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::PipelineStageFlagBits::eTransfer,
				{vk::ImageAspectFlagBits::eColor, i - 1, 1, 0, 1}
			);

			blit_image(i);
		}
		core->env.debug_marker.end_region(command_buffer);

		// transit last layer of downsample chain
		command_buffer.layout_transit(
			downsample_image,
			vk::ImageLayout::eGeneral,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			{vk::ImageAspectFlagBits::eColor, bloom_downsample_count - 1, 1, 0, 1}
		);

		// accumulate pipeline
		core->env.debug_marker.begin_region(command_buffer, "Accumulate", {1.0, 1.0, 0.0, 1.0});
		{
			command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, pipeline.bloom_pipeline.bloom_acc_pipeline);
			for (int i = bloom_downsample_count - 3; i >= 0; i--)
			{
				const auto extent = rt.bloom_rt.extents[i + 1];

				command_buffer->bindDescriptorSets(
					vk::PipelineBindPoint::eCompute,
					pipeline.bloom_pipeline.bloom_acc_pipeline_layout,
					0,
					{rt.bloom_rt.bloom_acc_descriptor_sets[i]},
					{}
				);

				command_buffer->dispatch(ceil((float)extent.width / 16), ceil((float)extent.height / 16), 1);

				command_buffer.layout_transit(
					upsample_image,
					vk::ImageLayout::eGeneral,
					vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::AccessFlagBits::eShaderWrite,
					vk::AccessFlagBits::eShaderRead,
					vk::PipelineStageFlagBits::eComputeShader,
					vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
					{vk::ImageAspectFlagBits::eColor, (uint32_t)i, 1, 0, 1},
					vk::DependencyFlagBits::eByRegion
				);
			}
		}
		core->env.debug_marker.end_region(command_buffer);
	}

	// Sync
	command_buffer.layout_transit(
		rt.lighting_rt.luminance,
		vk::ImageLayout::eGeneral,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eShaderRead,
		vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eFragmentShader,
		{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
		vk::DependencyFlagBits::eByRegion,
		c_queue_family,
		g_queue_family
	);
	core->env.debug_marker.end_region(command_buffer);
}

void App_render_logic::draw_composite(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto  draw_extent    = vk::Rect2D({0, 0}, core->env.swapchain.extent);

	auto set_viewport = [=, this](bool flip)
	{
		if (flip)
			command_buffer.set_viewport(utility::flip_viewport(vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0)
			));
		else
			command_buffer.set_viewport(vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0));

		command_buffer.set_scissor(vk::Rect2D({0, 0}, core->env.swapchain.extent));
	};

	core->env.debug_marker.begin_region(command_buffer, "Render Composite", {0.0, 0.2, 1.0, 1.0});
	command_buffer.begin_render_pass(
		core->Pipeline_set.composite_pipeline.render_pass,
		core->render_targets[idx].composite_rt.framebuffer,
		draw_extent,
		{Composite_pipeline::clear_value}
	);
	{
		set_viewport(true);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->Pipeline_set.composite_pipeline.pipeline_layout,
			0,
			{core->render_targets[idx].composite_rt.descriptor_set}
		);

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, core->Pipeline_set.composite_pipeline.pipeline);
		command_buffer.draw(0, 4, 0, 1);
	}
	command_buffer.end_render_pass();
	core->env.debug_marker.end_region(command_buffer);
}

void App_render_logic::ui_logic()
{
	auto& params    = core->params;
	auto& swapchain = core->env.swapchain;

	params.camera_controller.update(ImGui::GetIO());

	// Main Control
	{
		ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always, {0, 0});
		ImGui::Begin("Main Control Window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
		{
			ImGui::Checkbox("Control", &show_control);
			ImGui::Checkbox("Statistics", &show_panel);
			ImGui::Checkbox("Feature List", &show_feature);
			ImGui::Checkbox("Scene Explorer", &show_scene_explorer);
		}
		ImGui::End();
	}

	// Status Display
	if (show_panel)
	{
		const float framerate = ImGui::GetIO().Framerate;
		const float dt        = ImGui::GetIO().DeltaTime;
		ImGui::SetNextWindowPos({20, (float)swapchain.extent.height - 20}, ImGuiCond_Always, {0, 1});
		ImGui::Begin(
			"Status Display Window",
			nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize
		);
		{
			ImGui::Text("Objects: G=%d/S=%d", gbuffer_object_count.load(), shadow_object_count.load());
			ImGui::Text("Tris: G=%d/S=%d", gbuffer_vertex_count / 3, shadow_vertex_count / 3);
			ImGui::Text("FPS: %.1f", framerate);
			ImGui::Text("DT: %.1fms", dt * 1000);
			ImGui::Text("CPU Time: %.0fus", (gbuffer_cpu_time + shadow_cpu_time) * 1000);
		}
		ImGui::End();
	}

	// Control
	if (show_control)
	{
		if (ImGui::Begin("Control", &show_control, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::SeparatorText("Lighting");
			{
				ImGui::ColorEdit3("Light Color", &params.light_color.x, ImGuiColorEditFlags_DisplayHSV);
				ImGui::SliderFloat(
					"Light Intensity",
					&params.light_intensity,
					0.0,
					10000.0,
					"%.3f nits",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);
				ImGui::SliderFloat("Light Height", &params.sunlight_pitch, 0, 90, "%.1fdeg");
				ImGui::SliderFloat("Light Direction", &params.sunlight_yaw, 0, 360, "%.1fdeg");

				ImGui::SliderFloat("Exposure", &params.exposure_ev, -6, 6, "%.1fEV");

				ImGui::SliderFloat(
					"Emissive Brightness",
					&params.emissive_brightness,
					0.001,
					10000,
					"%.3f nits",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);

				ImGui::SliderFloat(
					"Skybox Brightness",
					&params.skybox_brightness,
					0.001,
					10000,
					"%.3f nits",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);

				ImGui::SliderFloat(
					"Bloom Intensity",
					&params.bloom_intensity,
					0.001,
					10,
					"%.3fx",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);

				ImGui::SliderFloat(
					"Bloom Start",
					&params.bloom_start,
					0.2,
					params.bloom_end,
					"%.2fx",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);

				ImGui::SliderFloat(
					"Bloom End",
					&params.bloom_end,
					params.bloom_start,
					100,
					"%.2fx",
					ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
				);

				ImGui::SliderFloat("Adapt Speed", &params.adapt_speed, 0.01, 5, "%.2fx");
			}

			ImGui::SeparatorText("Camera");
			{
				ImGui::SliderFloat("FOV", &params.fov, 1, 135, "%.1fdeg");
				{
					ImGui::BeginDisabled(params.auto_adjust_far_plane);
					ImGui::SliderFloat("Far", &params.far, 1, 10000, "%.2f", ImGuiSliderFlags_Logarithmic);
					ImGui::EndDisabled();
				}
				{
					ImGui::BeginDisabled(params.auto_adjust_near_plane);
					ImGui::SliderFloat("Near", &params.near, 0.01, params.far, "%.2f", ImGuiSliderFlags_Logarithmic);
					ImGui::EndDisabled();
				}
				ImGui::Checkbox("Auto Adjust Far Plane", &params.auto_adjust_far_plane);
				ImGui::Checkbox("Auto Adjust Near Plane", &params.auto_adjust_near_plane);
				ImGui::SliderFloat("CSM Blend Factor", &params.csm_blend_factor, 0, 1);
			}

			ImGui::SeparatorText("Debug");
			{
				ImGui::Checkbox("Visualize Shadow Perspective", &params.show_shadow_perspective);
				ImGui::BeginDisabled(!params.show_shadow_perspective);
				{
					ImGui::InputInt("Shadow Layer", &params.shadow_perspective_layer);
					params.shadow_perspective_layer = std::clamp<int>(params.shadow_perspective_layer, 0, 2);
				}
				ImGui::EndDisabled();
			}
		}
		ImGui::End();
	}

	// Feature list
	if (show_feature)
	{
		if (ImGui::Begin("Feature List", &show_feature, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto display_enable_status = [](const char* prefix, bool enabled)
			{
				ImGui::BulletText("%s: ", prefix);
				ImGui::SameLine();

				if (enabled)
					ImGui::TextColored({0.0, 1.0, 0.0, 1.0}, "Enabled");
				else
					ImGui::TextColored({1.0, 0.3, 0.0, 1.0}, "Disabled");
			};

			display_enable_status("10-bit Output", core->env.swapchain.feature.color_depth_10_enabled);
			display_enable_status("HDR Output", core->env.swapchain.feature.hdr_enabled);
		}
		ImGui::End();
	}

	if (show_scene_explorer)
	{
		if (ImGui::Begin("Scene Explorer", &show_scene_explorer, ImGuiWindowFlags_NoCollapse)) scene_explorer();
		ImGui::End();
	}

	ImGui::Render();
}

void App_render_logic::scene_explorer_node(size_t node_idx)
{
	const auto& node = core->params.model->nodes[node_idx];

	const auto tree_node_name = std::format("[Node {1}] {0}##node_{1}", node.name, node_idx);

	if (ImGui::TreeNode(tree_node_name.c_str()))
	{
		if (node.mesh_idx < core->params.model->meshes.size())
			ImGui::Text("Mesh: [Mesh %d] %s", node.mesh_idx, core->params.model->meshes[node.mesh_idx].name.c_str());

		for (auto child_node : node.children) scene_explorer_node(child_node);

		ImGui::TreePop();
	}
}

void App_render_logic::scene_explorer()
{
	const auto& model = *core->params.model;

	ImGui::BeginTabBar("Tabs");

	// Scene & Nodes
	ImGui::BeginTabItem("Scenes");

	// Iterate over all scenes
	for (auto i : Range(model.scenes.size()))
	{
		const auto& scene = model.scenes[i];

		// Iterate over all child nodes
		const auto header_name = std::format("[Scene {1}] {0}##scene_{1}", scene.name, i);

		if (ImGui::CollapsingHeader(header_name.c_str()))
		{
			for (auto node_idx : scene.nodes) scene_explorer_node(node_idx);
		}
	}

	ImGui::EndTabItem();

	ImGui::EndTabBar();
}

#pragma endregion

#pragma region "App Load Model Logic"

std::shared_ptr<Application_logic_base> App_load_model_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);

	// Start load thread
	auto load_work = [this]
	{
		core->env.log_msg("Loading model from \"{}\"...", load_path);
		core->params.model = std::make_shared<io::mesh::gltf::Model>();  // create new

		// Loader Context
		io::mesh::gltf::Loader_context loader_context;

		loader_context.allocator                     = core->env.allocator;
		loader_context.command_pool                  = Command_pool(core->env.device, core->env.g_family_idx);
		loader_context.device                        = core->env.device;
		loader_context.transfer_queue                = core->env.t_queue;
		loader_context.physical_device               = core->env.physical_device;
		loader_context.texture_descriptor_set_layout = core->Pipeline_set.gbuffer_pipeline.descriptor_set_layout_texture;
		loader_context.albedo_only_layout            = core->Pipeline_set.shadow_pipeline.descriptor_set_layout_texture;

		loader_context.load_stage   = &load_stage;
		loader_context.sub_progress = &sub_progress;

		const auto extension = std::filesystem::path(load_path).extension();

		try
		{
			if (load_path == LOAD_DEFAULT_MODEL_TOKEN)
			{
				core->params.model->load_gltf_memory(
					loader_context,
					std::span((uint8_t*)binary_resource::damaged_helmet_data, binary_resource::damaged_helmet_size)
				);
			}
			else
			{
				if (extension == ".gltf")
					core->params.model->load_gltf_ascii(loader_context, load_path);
				else
					core->params.model->load_gltf_bin(loader_context, load_path);
			}

			core->env.log_msg("Loaded model");
		}
		catch (const Exception& e)
		{
			load_err_msg       = e.msg;
			load_stage         = io::mesh::gltf::Load_stage::Error;
			core->params.model = nullptr;

			core->env.log_err("Load model failed, reason: {}", e.msg);
			return;
		}
	};

	load_thread = std::jthread(load_work);

	while (true)
	{
		// Poll SDL Events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;
		}

		core->ui_controller.imgui_new_frame();

		const auto render_result = core->render_one_frame(
			[this](uint32_t idx)
			{
				draw(idx);
			}
		);

		if (!render_result) return nullptr;

		if (quit)
		{
			if (load_stage == io::mesh::gltf::Load_stage::Finished && core->params.hdri != nullptr)
			{
				return std::make_shared<App_render_logic>(core);
			}
			else
				return std::make_shared<App_idle_logic>(core);
		}
	}
}

void App_load_model_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_load_model_logic::ui_logic()
{
	ImGui::SetNextWindowPos(ImVec2(core->env.swapchain.extent.width / 2.0, core->env.swapchain.extent.height / 2.0), ImGuiCond_Always, {0.5, 0.5});
	if (ImGui::Begin(
			"##Load Window",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		))
	{
		const char* display_msg      = "";
		const float display_progress = ((int)load_stage - 1 + sub_progress) / 3.0f;

		switch (load_stage)
		{
		case vklib_hpp::io::mesh::gltf::Load_stage::Uninitialized:
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Tinygltf_loading:
			display_msg = "TinyGltf Loading Data";
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Load_material:
			display_msg = "Loading Material";
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Load_mesh:
			display_msg = "Loading Mesh";
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Finished:
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Error:
			display_msg = "Error!";
			break;
		}

		ImGui::Text("%s", display_msg);

		if (load_stage == vklib_hpp::io::mesh::gltf::Load_stage::Error)
		{
			ImGui::BulletText("%s", load_err_msg.c_str());
			if (ImGui::Button("OK")) quit = true;
		}
		else if (load_stage == vklib_hpp::io::mesh::gltf::Load_stage::Finished)
		{
			quit = true;
		}
		else
		{
			// Show Progress bar
			ImGui::ProgressBar(display_progress, ImVec2(core->env.swapchain.extent.width / 3.0f, 0));
		}
	}
	ImGui::End();

	ImGui::Render();
}

#pragma endregion

#pragma region "App Load HDRI Logic"

std::shared_ptr<Application_logic_base> App_load_hdri_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);

	auto draw_logic = [&, this]() -> Application_logic_base*
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			if (event.type == SDL_QUIT) return nullptr;
		}

		core->ui_controller.imgui_new_frame();

		const auto render_result = core->render_one_frame(
			[this](uint32_t idx)
			{
				draw(idx);
			}
		);

		if (!render_result) return nullptr;

		return this;
	};

	// render one frame before freeze
	if (draw_logic() == nullptr) return nullptr;
	if (draw_logic() == nullptr) return nullptr;

	core->env.device->waitIdle();

	// load HDRI
	try
	{
		core->env.log_msg("Loading HDRi from \"{}\"...", load_path);

		core->params.hdri = std::make_shared<Hdri_resource>();
		io::images::Stbi_image_utility hdri_image;

		// load image
		{
			auto hdri_command_buffer = core->env.command_buffer[0];

			hdri_command_buffer.begin();

			const auto staging_buffer
				= load_path == LOAD_DEFUALT_HDRI_TOKEN
					? hdri_image.load_hdri(core->env.allocator, hdri_command_buffer, binary_resource::builtin_hdr_span)
					: hdri_image.load_hdri(core->env.allocator, hdri_command_buffer, load_path);

			hdri_command_buffer.end();

			const auto submit_command_buffers = Command_buffer::to_array({hdri_command_buffer});
			core->env.t_queue.submit(vk::SubmitInfo().setCommandBuffers(submit_command_buffers));

			core->env.device->waitIdle();
		}

		const Image_view hdri_view(
			core->env.device,
			hdri_image.image,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);

		core->params.hdri->generate(core->env, hdri_view, 1024, core->Pipeline_set.lighting_pipeline.skybox_input_layout);

		load_state = Load_state::Load_success;
		core->env.log_msg("Loaded HDRi");
	}
	catch (const Exception& e)
	{
		load_fail_reason = std::format("{} at {}", e.msg, e.loc.function_name());
		load_state       = Load_state::Load_failed;

		core->env.log_err("Load HDRi failed, reason: {}", e.msg);

		// cleanup
		core->params.hdri = nullptr;
	}

	// After load
	while (true)
	{
		if (draw_logic() == nullptr) return nullptr;

		if (load_state == Load_state::Quit || load_state == Load_state::Load_success)
		{
			if (core->params.model != nullptr)
			{
				return std::make_shared<App_render_logic>(core);
			}
			else
				return std::make_shared<App_idle_logic>(core);
		}
	}
}

void App_load_hdri_logic::draw(uint32_t idx)
{
	const auto& command_buffer = core->env.command_buffer[idx];

	command_buffer.begin();
	{
		ui_logic();
		core->ui_controller.imgui_draw(core->env, idx, true);
	}
	command_buffer.end();
}

void App_load_hdri_logic::ui_logic()
{
	ImGui::SetNextWindowPos(ImVec2(core->env.swapchain.extent.width / 2.0, core->env.swapchain.extent.height / 2.0), ImGuiCond_Always, {0.5, 0.5});
	if (ImGui::Begin(
			"##Load Window",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
		))
	{
		switch (load_state)
		{
		case Load_state::Start:
			ImGui::Text("Loading HDRI");
			ImGui::Text("Window may freeze, please wait.");
			break;
		case Load_state::Load_failed:
		{
			ImGui::Text("Load Failed:\n%s", load_fail_reason.c_str());
			if (ImGui::Button("OK")) load_state = Load_state::Quit;
			break;
		}
		case Load_state::Load_success:
		case Load_state::Quit:
			break;
		}
	}
	ImGui::End();

	ImGui::Render();
}

#pragma endregion