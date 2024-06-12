#include "application.hpp"

void Model_renderer::render_node(
	const io::mesh::gltf::Model&               model,
	uint32_t                                   idx,
	const algorithm::frustum_culling::Frustum& frustum,
	const glm::vec3&                           eye_position,
	const glm::vec3&                           eye_path,
	float&                                     near,
	float&                                     far
)
{
	const auto& node = model.nodes[idx];

	for (auto idx : node.children)
	{
		render_node(model, idx, frustum, eye_position, eye_path, near, far);
	}

	if (node.mesh_idx >= model.meshes.size()) return;

	const auto& mesh = model.meshes[node.mesh_idx];

	// iterates over all primitives
	for (auto i : Range(mesh.primitives.size()))
	{
		const auto& primitive = mesh.primitives[i];

		if (primitive.material_idx >= model.materials.size()) continue;

		if (!frustum.utility_no_cull)
		{
			const auto &min = primitive.min, &max = primitive.max;

			auto edge_points = algorithm::generate_boundaries(min, max);

			glm::vec3 min_coord(std::numeric_limits<float>::max()), max_coord(-std::numeric_limits<float>::max());

			for (auto& pt : edge_points)
			{
				auto coord = node.transformation * glm::vec4(pt, 1.0);
				pt         = coord / coord.w;
				min_coord  = glm::min(min_coord, pt);
				max_coord  = glm::max(max_coord, pt);
			}

			const auto bounding_box = algorithm::frustum_culling::AABB::from_min_max(min_coord, max_coord);

			const auto edge_bounded
				= bounding_box.intersect_or_forward(frustum.bottom) && bounding_box.intersect_or_forward(frustum.top)
			   && bounding_box.intersect_or_forward(frustum.left) && bounding_box.intersect_or_forward(frustum.right);

			// Auto adjust far plane
			if (edge_bounded)
				for (const auto& pt : edge_points)
				{
					far  = std::max(far, glm::dot(eye_path, pt - eye_position));
					near = std::min(near, glm::dot(eye_path, pt - eye_position));
				}

			if (!edge_bounded || !bounding_box.intersect_or_forward(frustum.far)
				|| !bounding_box.intersect_or_forward(frustum.near))
				continue;
		}

		if (model.materials[primitive.material_idx].double_sided)
			double_sided.emplace_back(idx, i);
		else
			single_sided.emplace_back(idx, i);
	}
}

Model_renderer::Draw_result Model_renderer::render_gltf(
	const Command_buffer&                                       command_buffer,
	const io::mesh::gltf::Model&                                model,
	const algorithm::frustum_culling::Frustum&                  frustum,
	const glm::vec3&                                            eye_position,
	const glm::vec3&                                            eye_path,
	const Graphics_pipeline&                                    single_pipeline,
	const Graphics_pipeline&                                    double_pipeline,
	const Pipeline_layout&                                      pipeline_layout,
	const std::function<void(const io::mesh::gltf::Material&)>& bind_func
)
{
	utility::Cpu_timer timer;

	// Initialization
	{
		single_sided.clear(), double_sided.clear();
		single_sided.reserve(model.nodes.size()), double_sided.reserve(model.nodes.size());
	}

	float near = std::numeric_limits<float>::max(), far = -std::numeric_limits<float>::max();

	// recursive collect
	for (const auto& scene : model.scenes)
	{
		for (auto idx : scene.nodes) render_node(model, idx, frustum, eye_position, eye_path, near, far);
	}

	timer.start();

	//* Single Side

	uint32_t prev_node = -1, prev_vertex_buffer = -1, prev_material = -1;
	command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, single_pipeline);

	for (const auto& pair : single_sided)
	{
		const auto& node      = model.nodes[std::get<0>(pair)];
		const auto& primitive = model.meshes[node.mesh_idx].primitives[std::get<1>(pair)];

		if (prev_node != std::get<0>(pair))
		{
			auto push_constants = Gbuffer_pipeline::Model_matrix(node.transformation);
			command_buffer.push_constants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constants);

			prev_node = std::get<0>(pair);
		}

		const auto& vert = model.vertex_buffers[primitive.vertices_idx];

		if (primitive.material_idx != prev_material)
		{
			bind_func(model.materials[primitive.material_idx]);

			prev_material = primitive.material_idx;
		}

		if (primitive.vertices_idx != prev_vertex_buffer)
		{
			command_buffer.bind_vertex_buffers(0, {vert.buffer}, {0});
			prev_vertex_buffer = primitive.vertices_idx;
		}

		command_buffer.draw(0, vert.count, 0, 1);
	}

	//* Double Side

	command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, double_pipeline);

	for (const auto& pair : double_sided)
	{
		const auto& node      = model.nodes[std::get<0>(pair)];
		const auto& primitive = model.meshes[node.mesh_idx].primitives[std::get<1>(pair)];

		if (prev_node != std::get<0>(pair))
		{
			command_buffer.push_constants(
				pipeline_layout,
				vk::ShaderStageFlagBits::eVertex,
				Gbuffer_pipeline::Model_matrix(node.transformation)
			);

			prev_node = std::get<0>(pair);
		}

		const auto& vert = model.vertex_buffers[primitive.vertices_idx];

		if (primitive.material_idx != prev_material)
		{
			bind_func(model.materials[primitive.material_idx]);

			prev_material = primitive.material_idx;
		}

		if (primitive.vertices_idx != prev_vertex_buffer)
		{
			const auto bind_buffers = Buffer::to_array({vert.buffer});
			const auto bind_offsets = std::to_array<vk::DeviceSize>({0});
			command_buffer.bind_vertex_buffers(0, bind_buffers, bind_offsets);
			prev_vertex_buffer = primitive.vertices_idx;
		}

		command_buffer.draw(0, vert.count, 0, 1);
	}

	timer.end();

	return {near, far, timer.duration<std::chrono::milliseconds>()};
}