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

		ImGui::BulletText("Model: %s", core->source.model == nullptr ? "Not Loaded" : "Loaded");
		ImGui::BulletText("HDRI: %s", core->source.hdri == nullptr ? "Not Loaded" : "Loaded");

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
		ui_logic();

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

			core->recreate_swapchain();
		}

		// Record Command Buffer
		draw(image_idx);

		// wait for fences
		const auto wait_fence_result = core->env.device->waitForFences({core->render_targets.next_frame_fence}, true, 1e10);
		if (wait_fence_result != vk::Result::eSuccess) throw Exception("Fence wait timeout!");
		core->env.device->resetFences({core->render_targets.next_frame_fence});

		// Submit Command Buffers
		submit_commands(command_buffers[image_idx]);

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

void App_render_logic::submit_commands(const Command_buffer_set& set)
{
	const auto gbuffer_signal_semaphore = Semaphore::to_array({gbuffer_semaphore});
	const auto gbuffer_submit_buffer    = Command_buffer::to_array({set.gbuffer_command_buffer});
	const auto gbuffer_submit_info
		= vk::SubmitInfo().setCommandBuffers(gbuffer_submit_buffer).setSignalSemaphores(gbuffer_signal_semaphore);

	const auto  shadow_signal_semaphore = Semaphore::to_array({shadow_semaphore});
	const auto& shadow_submit_buffer    = Command_buffer::to_array({set.shadow_command_buffer});
	const auto  shadow_submit_info
		= vk::SubmitInfo().setCommandBuffers(shadow_submit_buffer).setSignalSemaphores(shadow_signal_semaphore);

	const auto lighting_signal_semaphore = Semaphore::to_array({lighting_semaphore});
	const auto lighting_wait_semaphore   = Semaphore::to_array({gbuffer_semaphore, shadow_semaphore});
	const auto lighting_wait_stages      = std::to_array<vk::PipelineStageFlags>(
        {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput}
    );
	const auto lighting_submit_buffers   = Command_buffer::to_array({set.lighting_command_buffer});
	const auto lighting_submit_info      = vk::SubmitInfo()
										  .setCommandBuffers(lighting_submit_buffers)
										  .setWaitDstStageMask(lighting_wait_stages)
										  .setWaitSemaphores(lighting_wait_semaphore)
										  .setSignalSemaphores(lighting_signal_semaphore);

	const auto compute_signal_semaphore = Semaphore::to_array({compute_semaphore});
	const auto compute_wait_semaphore   = Semaphore::to_array({lighting_semaphore});
	const auto compute_wait_stages      = std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
	const auto compute_submit_buffers   = Command_buffer::to_array({set.compute_command_buffer});
	const auto compute_submit_info      = vk::SubmitInfo()
										 .setCommandBuffers(compute_submit_buffers)
										 .setWaitDstStageMask(compute_wait_stages)
										 .setWaitSemaphores(compute_wait_semaphore)
										 .setSignalSemaphores(compute_signal_semaphore);

	const auto composite_signal_semaphore = Semaphore::to_array({composite_semaphore});
	const auto composite_wait_semaphore   = Semaphore::to_array({compute_semaphore, core->render_targets.acquire_semaphore});
	const auto composite_wait_stages      = std::to_array<vk::PipelineStageFlags>(
        {vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput}
    );
	const auto composite_submit_buffers = Command_buffer::to_array({set.composite_command_buffer});
	const auto composite_submit_info    = vk::SubmitInfo()
										   .setCommandBuffers(composite_submit_buffers)
										   .setWaitDstStageMask(composite_wait_stages)
										   .setWaitSemaphores(composite_wait_semaphore)
										   .setSignalSemaphores(composite_signal_semaphore);

	core->env.g_queue2.submit({shadow_submit_info});
	core->env.g_queue.submit({gbuffer_submit_info, lighting_submit_info});
	core->env.c_queue.submit({compute_submit_info});
	core->env.g_queue.submit({composite_submit_info}, core->render_targets.next_frame_fence);
}

void App_render_logic::draw(uint32_t idx)
{
	gbuffer_object_count = 0;
	gbuffer_vertex_count = 0;
	shadow_object_count  = 0;
	shadow_vertex_count  = 0;
	cpu_time             = 0;

	const auto& command_buffer_set = command_buffers[idx];

	utility::Cpu_timer timer;
	timer.start();

	draw_gbuffer(idx, command_buffer_set.gbuffer_command_buffer);
	update_uniforms(idx);
	draw_shadow(idx, command_buffer_set.shadow_command_buffer);
	draw_lighting(idx, command_buffer_set.lighting_command_buffer);
	compute_process(idx, command_buffer_set.compute_command_buffer);
	draw_swapchain(idx, command_buffer_set.composite_command_buffer);

	timer.end();
	cpu_time = timer.duration<std::chrono::microseconds>();
}

void App_render_logic::draw_gbuffer(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto draw_extent               = vk::Rect2D({0, 0}, core->env.swapchain.extent);
	const auto gbuffer_camera_param_prev = core->params.get_gbuffer_parameter(core->env);

	const auto gen_params = Drawcall_generator::Gen_params{
		core->source.model.get(),
		&animation_buffer,
		gbuffer_camera_param_prev.frustum,
		gbuffer_camera_param_prev.eye_position,
		gbuffer_camera_param_prev.eye_direction,
		glm::mat4(1.0),
		0
	};

	const auto gen_result = gbuffer_generator.generate(gen_params);

	const float far  = std::max(0.02f, gen_result.far);
	float       near = std::max(0.01f, gen_result.near);
	near             = std::min(near, far - 0.01f);
	near             = std::max(near, far / 200);

	if (core->params.auto_adjust_near_plane) core->params.near = near;
	if (core->params.auto_adjust_far_plane) core->params.far = far;

	gbuffer_object_count = gen_result.object_count;
	gbuffer_vertex_count = gen_result.vertex_count;
	scene_min_bound      = gen_result.min_bounding;
	scene_max_bound      = gen_result.max_bounding;

	// Update uniform buffer
	gbuffer_param                          = core->params.get_gbuffer_parameter(core->env);
	const auto gbuffer_camera_uniform_data = Gbuffer_pipeline::Camera_uniform{gbuffer_param.view_projection_matrix};
	const auto gbuffer_update = core->render_targets.render_target_set[idx].gbuffer_rt.update_uniform(gbuffer_camera_uniform_data);
	const auto gbuffer_update_info = gbuffer_update.write_info();
	core->env.device->updateDescriptorSets({gbuffer_update_info}, {});

	command_buffer.begin();
	core->env.debug_marker.begin_region(command_buffer, "Render Gbuffer", {0.0, 1.0, 1.0, 1.0});
	command_buffer.begin_render_pass(
		core->pipeline_set.gbuffer_pipeline.render_pass,
		core->render_targets[idx].gbuffer_rt.framebuffer,
		draw_extent,
		Gbuffer_pipeline::clear_values
	);
	{
		command_buffer.set_viewport(
			utility::flip_viewport(vk::Viewport(0, 0, core->env.swapchain.extent.width, core->env.swapchain.extent.height, 0.0, 1.0))
		);
		command_buffer.set_scissor(vk::Rect2D({0, 0}, core->env.swapchain.extent));

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->pipeline_set.gbuffer_pipeline.pipeline_layout,
			0,
			{core->render_targets[idx].gbuffer_rt.camera_uniform_descriptor_set}
		);

		auto bind_material = [this, command_buffer](auto drawcall)
		{
			command_buffer.bind_descriptor_sets(
				vk::PipelineBindPoint::eGraphics,
				core->pipeline_set.gbuffer_pipeline.pipeline_layout,
				1,
				{core->source.material_data[drawcall.primitive.material_idx].descriptor_set}
			);
		};

		auto bind_vertex = [this, command_buffer](auto drawcall)
		{
			const auto& model     = core->source.model;
			const auto& primitive = drawcall.primitive;
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

		const auto& single_sided = gbuffer_generator.get_single_sided_drawlist();
		const auto& double_sided = gbuffer_generator.get_double_sided_drawlist();

		const auto single_draw_params = Drawlist::Draw_params{
			command_buffer,
			core->pipeline_set.gbuffer_pipeline.single_sided_pipeline,
			core->pipeline_set.gbuffer_pipeline.single_sided_pipeline_alpha,
			core->pipeline_set.gbuffer_pipeline.single_sided_pipeline_blend,
			core->pipeline_set.gbuffer_pipeline.pipeline_layout,
			bind_material,
			bind_vertex,
			bind_vertex,
			bind_vertex
		};

		const auto double_draw_params = Drawlist::Draw_params{
			command_buffer,
			core->pipeline_set.gbuffer_pipeline.double_sided_pipeline,
			core->pipeline_set.gbuffer_pipeline.double_sided_pipeline_alpha,
			core->pipeline_set.gbuffer_pipeline.double_sided_pipeline_blend,
			core->pipeline_set.gbuffer_pipeline.pipeline_layout,
			bind_material,
			bind_vertex,
			bind_vertex,
			bind_vertex
		};

		single_sided.draw(single_draw_params);
		double_sided.draw(double_draw_params);
	}
	command_buffer.end_render_pass();
	core->env.debug_marker.end_region(command_buffer);
	command_buffer.end();
}

void App_render_logic::update_uniforms(uint32_t idx)
{
	const Composite_pipeline::Exposure_param composite_param{exp2(core->params.exposure_ev), core->params.bloom_intensity};

	Lighting_pipeline::Params lighting_params{};
	{
		lighting_params.view_projection_inv = glm::inverse(gbuffer_param.view_projection_matrix);
		lighting_params.camera_position     = gbuffer_param.eye_position;
		lighting_params.skybox_brightness   = core->params.skybox_brightness;
		lighting_params.emissive_brightness = core->params.emissive_brightness;
		lighting_params.sunlight_pos        = core->params.get_light_direction();
		lighting_params.sunlight_color      = glm::pow(core->params.light_color, glm::vec3(1 / 2.2)) * core->params.light_intensity;
	}

	std::array<Shadow_pipeline::Shadow_uniform, csm_count> shadow_uniforms;
	for (const auto csm_idx : Range(csm_count))
	{
		shadow_params[csm_idx] = core->params.get_shadow_parameters(
			glm::mix(core->params.near, core->params.far, csm_idx / 3.0f),
			glm::mix(core->params.near, core->params.far, (csm_idx + 1) / 3.0f),
			core->params.shadow_near[csm_idx],
			core->params.shadow_far[csm_idx],
			core->params.get_gbuffer_parameter(core->env)
		);
		shadow_uniforms[csm_idx] = Shadow_pipeline::Shadow_uniform{shadow_params[csm_idx].view_projection_matrix};

		lighting_params.shadow_size[csm_idx].texel_size = glm::vec2(1.0 / shadow_map_res[csm_idx]);
		lighting_params.shadow_size[csm_idx].view_size  = shadow_params[csm_idx].shadow_view_size;
		lighting_params.shadow[csm_idx]                 = shadow_params[csm_idx].view_projection_matrix;
	}

	const auto lighting_write  = core->render_targets.render_target_set[idx].lighting_rt.update_uniform(lighting_params);
	const auto composite_write = core->render_targets.render_target_set[idx].composite_rt.update_uniform(composite_param);
	const auto shadow_write    = core->render_targets.render_target_set[idx].shadow_rt.update_uniform(shadow_uniforms);
	std::array<vk::WriteDescriptorSet, csm_count> shadow_update_info;
	for (auto i : Range(csm_count)) shadow_update_info[i] = shadow_write[i].write_info();

	const auto write_sets
		= utility::join_array(std::to_array({lighting_write.write_info(), composite_write.write_info()}), shadow_update_info);

	core->env.device->updateDescriptorSets(write_sets, {});
}

void App_render_logic::draw_shadow(uint32_t idx, const Command_buffer& command_buffer)
{
	command_buffer.begin();
	for (const auto csm_idx : Range(csm_count))
	{
		core->env.debug_marker.begin_region(command_buffer, std::format("Render Shadow Map, Level {}", csm_idx), {1.0, 1.0, 0.0, 1.0});
		command_buffer.begin_render_pass(
			core->pipeline_set.shadow_pipeline.render_pass,
			core->render_targets[idx].shadow_rt.shadow_framebuffers[csm_idx],
			vk::Rect2D({0, 0}, {shadow_map_res[csm_idx], shadow_map_res[csm_idx]}),
			{Shadow_pipeline::clear_value},
			vk::SubpassContents::eInline
		);
		{
			// Set Viewport and Scissors
			command_buffer.set_viewport(
				utility::flip_viewport(vk::Viewport(0, 0, shadow_map_res[csm_idx], shadow_map_res[csm_idx], 0.0, 1.0))
			);

			command_buffer.set_scissor({
				{0,					   0					  },
				{shadow_map_res[csm_idx], shadow_map_res[csm_idx]}
			});

			// Bind Matrices
			command_buffer.bind_descriptor_sets(
				vk::PipelineBindPoint::eGraphics,
				core->pipeline_set.shadow_pipeline.pipeline_layout,
				0,
				{core->render_targets[idx].shadow_rt.shadow_matrix_descriptor_set[csm_idx]}
			);

			auto bind_material = [=, this](Drawcall drawcall)
			{
				command_buffer.bind_descriptor_sets(
					vk::PipelineBindPoint::eGraphics,
					core->pipeline_set.shadow_pipeline.pipeline_layout,
					1,
					{core->source.material_data[drawcall.primitive.material_idx].albedo_only_descriptor_set}
				);
			};

			auto bind_vertex = [=, this](Drawcall drawcall)
			{
				const auto& model     = *core->source.model;
				const auto& primitive = drawcall.primitive;

				command_buffer->bindVertexBuffers(
					0,
					{model.vec3_buffers[primitive.position_buffer], model.vec2_buffers[primitive.uv_buffer]},
					{primitive.position_offset * sizeof(glm::vec3), primitive.uv_offset * sizeof(glm::vec2)}
				);
			};

			auto bind_vertex_opaque = [=, this](Drawcall drawcall)
			{
				const auto& model     = *core->source.model;
				const auto& primitive = drawcall.primitive;

				command_buffer->bindVertexBuffers(
					0,
					{model.vec3_buffers[primitive.position_buffer]},
					{primitive.position_offset * sizeof(glm::vec3)}
				);
			};

			auto gen_params = Drawcall_generator::Gen_params{
				core->source.model.get(),
				&animation_buffer,
				shadow_params[csm_idx].frustum,
				shadow_params[csm_idx].eye_position,
				shadow_params[csm_idx].eye_direction,
				glm::mat4(1.0),
				0
			};

			const auto gen_result = shadow_generator[csm_idx].generate(gen_params);

			const float near = std::min((gen_result.near + gen_result.far) / 2.0f - 0.01f, gen_result.near);
			const float far  = std::max((gen_result.near + gen_result.far) / 2.0f + 0.01f, gen_result.far);

			core->params.shadow_far[csm_idx]  = far;
			core->params.shadow_near[csm_idx] = near;

			shadow_object_count += gen_result.object_count;
			shadow_vertex_count += gen_result.vertex_count;

			const auto& single_sided = shadow_generator[csm_idx].get_single_sided_drawlist();
			const auto& double_sided = shadow_generator[csm_idx].get_double_sided_drawlist();

			const auto single_draw_params = Drawlist::Draw_params{
				command_buffer,
				core->pipeline_set.shadow_pipeline.single_sided_pipeline,
				core->pipeline_set.shadow_pipeline.single_sided_pipeline_alpha,
				core->pipeline_set.shadow_pipeline.single_sided_pipeline_blend,
				core->pipeline_set.shadow_pipeline.pipeline_layout,
				bind_material,
				bind_vertex_opaque,
				bind_vertex,
				bind_vertex
			};

			const auto double_draw_params = Drawlist::Draw_params{
				command_buffer,
				core->pipeline_set.shadow_pipeline.double_sided_pipeline,
				core->pipeline_set.shadow_pipeline.double_sided_pipeline_alpha,
				core->pipeline_set.shadow_pipeline.double_sided_pipeline_blend,
				core->pipeline_set.shadow_pipeline.pipeline_layout,
				bind_material,
				bind_vertex_opaque,
				bind_vertex,
				bind_vertex
			};

			single_sided.draw(single_draw_params);
			double_sided.draw(double_draw_params);
		}
		command_buffer.end_render_pass();
		core->env.debug_marker.end_region(command_buffer);
	}
	command_buffer.end();
}

void App_render_logic::compute_process(uint32_t idx, const Command_buffer& command_buffer)
{
	command_buffer.begin();
	compute_auto_exposure(idx, command_buffer);
	compute_bloom(idx, command_buffer);
	command_buffer.end();
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
	command_buffer.begin();
	core->env.debug_marker.begin_region(command_buffer, "Render Lighting", {0.0, 0.0, 1.0, 1.0});
	command_buffer.begin_render_pass(
		core->pipeline_set.lighting_pipeline.render_pass,
		core->render_targets[idx].lighting_rt.framebuffer,
		draw_extent,
		{Lighting_pipeline::clear_value}
	);
	{
		set_viewport(false);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->pipeline_set.lighting_pipeline.pipeline_layout,
			0,
			{core->render_targets[idx].lighting_rt.input_descriptor_set, core->source.hdri->descriptor_set}
		);

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, core->pipeline_set.lighting_pipeline.pipeline);

		command_buffer.draw(0, 4, 0, 1);
	}
	command_buffer.end_render_pass();
	core->env.debug_marker.end_region(command_buffer);
	command_buffer.end();
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
		command_buffer.bind_pipeline(
			vk::PipelineBindPoint::eCompute,
			core->pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline
		);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eCompute,
			core->pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline_layout,
			0,
			{core->render_targets.auto_exposure_rt.luminance_avg_descriptor_sets[idx]}
		);

		const Auto_exposure_compute_pipeline::Luminance_params params{
			Auto_exposure_compute_pipeline::min_luminance,
			Auto_exposure_compute_pipeline::max_luminance
		};

		command_buffer.push_constants(
			core->pipeline_set.auto_exposure_pipeline.luminance_avg_pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			params,
			0
		);

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
		command_buffer.bind_pipeline(vk::PipelineBindPoint::eCompute, core->pipeline_set.auto_exposure_pipeline.lerp_pipeline);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eCompute,
			core->pipeline_set.auto_exposure_pipeline.lerp_pipeline_layout,
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

		command_buffer.push_constants(
			core->pipeline_set.auto_exposure_pipeline.lerp_pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			params,
			0
		);

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

void App_render_logic::draw_swapchain(uint32_t idx, const Command_buffer& command_buffer)
{
	command_buffer.begin();
	{
		// Draw Composite
		draw_composite(idx, command_buffer);

		// Draw UI
		core->env.debug_marker.begin_region(command_buffer, "Draw IMGUI", {0.7, 0.0, 1.0, 1.0});
		core->ui_controller.imgui_draw(core->env, command_buffer, idx, false);
		core->env.debug_marker.end_region(command_buffer);
	}
	command_buffer.end();
}

void App_render_logic::compute_bloom(uint32_t idx, const Command_buffer& command_buffer)
{
	const auto  g_queue_family = core->env.g_family_idx, c_queue_family = core->env.c_family_idx;

	const auto& rt        = core->render_targets[idx];
	const auto& pipeline  = core->pipeline_set;
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
		core->pipeline_set.composite_pipeline.render_pass,
		core->render_targets[idx].composite_rt.framebuffer,
		draw_extent,
		{Composite_pipeline::clear_value}
	);
	{
		set_viewport(true);

		command_buffer.bind_descriptor_sets(
			vk::PipelineBindPoint::eGraphics,
			core->pipeline_set.composite_pipeline.pipeline_layout,
			0,
			{core->render_targets[idx].composite_rt.descriptor_set}
		);

		command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, core->pipeline_set.composite_pipeline.pipeline);
		command_buffer.draw(0, 4, 0, 1);
	}
	command_buffer.end_render_pass();
	core->env.debug_marker.end_region(command_buffer);
}

void App_render_logic::stat_panel()
{
	const auto& swapchain = core->env.swapchain;

	const float framerate = ImGui::GetIO().Framerate;
	const float dt        = ImGui::GetIO().DeltaTime;
	ImGui::SetNextWindowPos({20, (float)swapchain.extent.height - 20}, ImGuiCond_Always, {0, 1});
	ImGui::Begin(
		"Status Display Window",
		nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize
	);
	{
		ImGui::Text("Objects: G=%d/S=%d", gbuffer_object_count, shadow_object_count);
		ImGui::Text("Tris: G=%d/S=%d", gbuffer_vertex_count / 3, shadow_vertex_count / 3);
		ImGui::Text("FPS: %.1f", framerate);
		ImGui::Text("DT: %.1fms", dt * 1000);
		ImGui::Text("CPU Time: %.0fus", cpu_time);
	}
	ImGui::End();
}

void App_render_logic::control_tab()
{
	auto& params = core->params;

	// center camera view
	if (ImGui::Button("Center Camera View"))
		core->params.camera_controller.target_eye_center = (scene_min_bound + scene_max_bound) / 2.0f;

	ImGui::SeparatorText("Lighting");
	{
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
		}
		ImGui::Separator();
		{
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
		}
		ImGui::Separator();
		{
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
		}

		ImGui::Separator();
		ImGui::SliderFloat("Adapt Speed", &params.adapt_speed, 0.01, 5, "%.2fx");
		ImGui::SliderFloat("Exposure", &params.exposure_ev, -6, 6, "%.1fEV");
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

void App_render_logic::system_tab()
{
	ImGui::Checkbox("Stats Panel", &show_panel);

	// Feature
	{
		ImGui::SeparatorText("Features");

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
}

void App_render_logic::ui_logic()
{
	auto&       params    = core->params;
	const auto& swapchain = core->env.swapchain;

	params.camera_controller.update(ImGui::GetIO());

	ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always, {0, 0});
	ImGui::SetNextWindowSizeConstraints({0.0f, 0.0f}, {-1.0f, swapchain.extent.height - 200.0f});
	if (ImGui::Begin(
			"##Main Control Window",
			NULL,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize
		))
	{
		if (ImGui::BeginTabBar("##Main Tab"))
		{
			if (ImGui::BeginTabItem("System"))
			{
				system_tab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Control"))
			{
				control_tab();
				ImGui::EndTabItem();
			}

			if (!core->source.model->animations.empty())
				if (ImGui::BeginTabItem("Animation"))
				{
					animation_tab();
					ImGui::EndTabItem();
				}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	// Status Display
	if (show_panel) stat_panel();

	ImGui::Render();
}

void App_render_logic::update_animation()
{
	if (selected_animation < 0) return;

	const auto&  model     = *core->source.model;
	const auto&  animation = model.animations[selected_animation];
	const double now       = ImGui::GetTime();

	// assign animation time
	if (animation_playing)
	{
		animation_time = now - animation_start_time;
		animation_time *= animation_rate;

		if (animation_time > animation.end_time)
		{
			if (animation_cycle)
				animation_start_time = now;
			else
				animation_playing = false;
		}
	}

	animation.set_transformation(
		animation_time,
		[&](uint32_t idx)
		{
			auto find = animation_buffer.find(idx);
			if (find == animation_buffer.end()) find = animation_buffer.emplace(idx, model.nodes[idx].transformation).first;

			return std::ref(find->second);
		}
	);
}

void App_render_logic::animation_tab()
{
	const auto&       model   = *core->source.model;
	const std::string preview = selected_animation == -1
								  ? "Disabled"
								  : std::format("[{}] {}", selected_animation, model.animations[selected_animation].name);

	if (ImGui::BeginCombo("Animation", preview.c_str()))
	{
		// Disable option
		if (ImGui::Selectable("Disabled", selected_animation == -1))
		{
			selected_animation = -1;
			animation_playing  = false;
			animation_buffer.clear();
		}

		// Iterate over all animations
		for (auto i : Range<int>(model.animations.size()))
		{
			const auto& animation = model.animations[i];

			const bool selected = ImGui::Selectable(std::format("[{}] {}", i, animation.name).c_str(), selected_animation == i);

			if (selected)
			{
				selected_animation = i;
				animation_playing  = false;
				animation_buffer.clear();
			}
		}

		ImGui::EndCombo();
	}

	if (selected_animation < 0) return;

	const auto& animation = model.animations[selected_animation];
	const double now       = ImGui::GetTime();

	ImGui::BulletText("Start Time: %.2fs", animation.start_time);
	ImGui::BulletText("End Time: %.2fs", animation.end_time);

	ImGui::BeginDisabled(animation_playing);
	{
		ImGui::SliderFloat("Time", &animation_time, std::min(0.0f, animation.start_time), animation.end_time, "%.3fs");
	}
	ImGui::EndDisabled();

	if (ImGui::SliderFloat("Animation Rate", &animation_rate, 0.1, 2.0, "%.1fx", ImGuiSliderFlags_AlwaysClamp))
	{
		animation_start_time = now - animation_time / animation_rate;
	}

	ImGui::Checkbox("Cycle Animation", &animation_cycle);

	// Start/stop button
	if (ImGui::Button(animation_playing ? "Stop" : "Play"))
	{
		if (animation_playing)
			animation_playing = false;
		else
		{
			animation_playing    = true;
			animation_start_time = now - animation_time / animation_rate;
		}
	}
}

#pragma endregion

#pragma region "App Load Model Logic"

void App_load_model_logic::load_thread_work()
{
	core->env.log_msg("Loading model from \"{}\"...", load_path);
	core->source.model = std::make_shared<io::mesh::gltf::Model>();  // create new

	// Loader Context
	io::mesh::gltf::Loader_context loader_context;

	loader_context.allocator               = core->env.allocator;
	loader_context.command_pool            = Command_pool(core->env.device, core->env.g_family_idx);
	loader_context.device                  = core->env.device;
	loader_context.transfer_queue          = core->env.t_queue;
	loader_context.physical_device         = core->env.physical_device;
	loader_context.load_stage              = &load_stage;
	loader_context.sub_progress            = &sub_progress;
	loader_context.config.enable_anistropy = core->env.features.anistropy_enabled;
	loader_context.config.max_anistropy    = std::min(8.0f, core->env.features.max_anistropy);

	const auto extension = std::filesystem::path(load_path).extension();

	try
	{
		if (load_path == LOAD_DEFAULT_MODEL_TOKEN)
		{
			core->source.model->load_gltf_memory(
				loader_context,
				std::span((uint8_t*)binary_resource::damaged_helmet_data, binary_resource::damaged_helmet_size)
			);
		}
		else
		{
			if (extension == ".gltf")
				core->source.model->load_gltf_ascii(loader_context, load_path);
			else
				core->source.model->load_gltf_bin(loader_context, load_path);
		}

		core->source.generate_material_data(core->env, core->pipeline_set);

		core->env.log_msg("Loaded model");
	}
	catch (const Exception& e)
	{
		load_err_msg       = e.msg;
		load_stage         = io::mesh::gltf::Load_stage::Error;
		core->source.model = nullptr;

		core->env.log_err("Load model failed, reason: {}", e.msg);
		return;
	}
}

std::shared_ptr<Application_logic_base> App_load_model_logic::work()
{
	SDL_EventState(SDL_DROPBEGIN, SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
	SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);
	SDL_EventState(SDL_DROPCOMPLETE, SDL_DISABLE);

	load_thread = std::jthread(
		[this]
		{
			this->load_thread_work();
		}
	);

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
			if (load_stage == io::mesh::gltf::Load_stage::Finished && core->source.hdri != nullptr)
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
		float       display_progress = sub_progress;

		switch (load_stage)
		{
		case vklib_hpp::io::mesh::gltf::Load_stage::Uninitialized:
			break;
		case vklib_hpp::io::mesh::gltf::Load_stage::Tinygltf_loading:
			display_msg = "TinyGltf Loading Data";
			display_progress = -1;
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
			if (display_progress >= 0)
				ImGui::ProgressBar(display_progress, ImVec2(core->env.swapchain.extent.width / 3.0f, 0));
			else
				ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(core->env.swapchain.extent.width / 3.0f, 0));
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

		core->source.hdri = std::make_shared<Hdri_resource>();
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

		core->source.hdri->generate(core->env, hdri_view, 1024, core->pipeline_set.lighting_pipeline.skybox_input_layout);

		load_state = Load_state::Load_success;
		core->env.log_msg("Loaded HDRi");
	}
	catch (const Exception& e)
	{
		load_fail_reason = std::format("{} at {}", e.msg, e.loc.function_name());
		load_state       = Load_state::Load_failed;

		core->env.log_err("Load HDRi failed, reason: {}", e.msg);

		// cleanup
		core->source.hdri = nullptr;
	}

	// After load
	while (true)
	{
		if (draw_logic() == nullptr) return nullptr;

		if (load_state == Load_state::Quit || load_state == Load_state::Load_success)
		{
			if (core->source.model != nullptr)
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
