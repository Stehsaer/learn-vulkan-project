#include "model-renderer.hpp"

void Model_renderer::render_node(
	const io::mesh::gltf::Model&               model,
	uint32_t                                   node_idx,
	const algorithm::frustum_culling::Frustum& frustum,
	const glm::vec3&                           eye_position,
	const glm::vec3&                           eye_path,
	float&                                     near,
	float&                                     far
)
{
	const auto& node = model.nodes[node_idx];

	// recursive render node
	for (auto children_idx : node.children)
		render_node(model, children_idx, frustum, eye_position, eye_path, near, far);

	// ignore nodes without a mesh
	if (node.mesh_idx >= model.meshes.size()) return;

	const auto& mesh_idx = model.meshes[node.mesh_idx];

	// iterates over all primitives
	for (auto i : Range(mesh_idx.primitives.size()))
	{
		const auto& primitive = mesh_idx.primitives[i];

		// ignore primitives without material
		if (primitive.material_idx >= model.materials.size()) continue;

		const auto material_idx = primitive.material_idx;

		const auto &min = primitive.min, &max = primitive.max;

		/* Construct AABB after transformation */

		auto edge_points = algorithm::generate_boundaries(min, max);

		glm::vec3 min_coord(std::numeric_limits<float>::max()), max_coord(-std::numeric_limits<float>::max());

		for (auto& pt : edge_points)
		{
			const auto coord = node.transformation * glm::vec4(pt, 1.0);
			pt               = coord / coord.w;
			min_coord        = glm::min(min_coord, pt);
			max_coord        = glm::max(max_coord, pt);
		}

		const auto bounding_box = algorithm::frustum_culling::AABB::from_min_max(min_coord, max_coord);

		const auto edge_bounded
			= bounding_box.intersect_or_forward(frustum.bottom) && bounding_box.intersect_or_forward(frustum.top)
		   && bounding_box.intersect_or_forward(frustum.left) && bounding_box.intersect_or_forward(frustum.right);

		// Calculate far & near plane

		if (edge_bounded)
			for (const auto& pt : edge_points)
			{
				far  = std::max(far, glm::dot(eye_path, pt - eye_position));
				near = std::min(near, glm::dot(eye_path, pt - eye_position));
			}

		if (!edge_bounded || !bounding_box.intersect_or_forward(frustum.far)
			|| !bounding_box.intersect_or_forward(frustum.near))
			continue;

		Renderer_drawcall drawcall{node_idx, primitive};

		if (model.materials[primitive.material_idx].double_sided)
			double_sided.emplace_back(drawcall);
		else
			single_sided.emplace_back(drawcall);
	}
}

Model_renderer::Draw_result Model_renderer::render_gltf(
	const Command_buffer&                                        command_buffer,
	const io::mesh::gltf::Model&                                 model,
	const algorithm::frustum_culling::Frustum&                   frustum,
	const glm::vec3&                                             eye_position,
	const glm::vec3&                                             eye_path,
	const Graphics_pipeline&                                     single_pipeline,
	const Graphics_pipeline&                                     double_pipeline,
	const Pipeline_layout&                                       pipeline_layout,
	const std::function<void(const io::mesh::gltf::Material&)>&  bind_func,
	const std::function<void(const io::mesh::gltf::Primitive&)>& bind_vertex_func,
	bool                                                         sort_drawcall
)
{
	utility::Cpu_timer timer;
	timer.start();

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

	// sort drawcalls

	if (sort_drawcall)
	{
		std::sort(single_sided.begin(), single_sided.end());
		std::sort(double_sided.begin(), double_sided.end());
	}

	// draw a drawlist
	auto draw = [=](const std::vector<Renderer_drawcall>& draw_list)
	{
		uint32_t prev_node = -1, prev_vertex_buffer = -1, prev_offset = -1, prev_material = -1;

		for (const auto& drawcall : draw_list)
		{
			const auto& node = model.nodes[drawcall.node_idx];

			if (prev_node != drawcall.node_idx)
			{
				auto push_constants = Gbuffer_pipeline::Model_matrix(node.transformation);
				command_buffer.push_constants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constants);
				prev_node = drawcall.node_idx;
			}

			if (drawcall.primitive.material_idx != prev_material)
			{
				bind_func(model.materials[drawcall.primitive.material_idx]);
				prev_material = drawcall.primitive.material_idx;
			}

			if (drawcall.primitive.position_buffer != prev_vertex_buffer || drawcall.primitive.position_offset != prev_offset)
			{
				bind_vertex_func(drawcall.primitive);
				prev_vertex_buffer = drawcall.primitive.position_buffer;
				prev_offset        = drawcall.primitive.position_offset;
			}

			command_buffer.draw(0, drawcall.primitive.vertex_count, 0, 1);
		}
	};

	//* Single Side

	command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, single_pipeline);
	draw(single_sided);

	//* Double Side

	command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, double_pipeline);
	draw(double_sided);

	timer.end();

	return {near, far, timer.duration<std::chrono::milliseconds>()};
}