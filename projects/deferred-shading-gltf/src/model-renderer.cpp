#include "model-renderer.hpp"

void Drawlist::emplace(const Drawcall& drawcall, io::mesh::gltf::Alpha_mode mode)
{
	switch (mode)
	{
	case io::mesh::gltf::Alpha_mode::Opaque:
		opaque.emplace_back(drawcall);
		break;
	case io::mesh::gltf::Alpha_mode::Mask:
		mask.emplace_back(drawcall);
		break;
	case io::mesh::gltf::Alpha_mode::Blend:
		blend.emplace_back(drawcall);
		break;
	}
}

void Drawlist::draw(const Draw_params& params) const
{
	auto draw = [&](const std::vector<Drawcall>&                draw_list,
					const Graphics_pipeline&                    pipeline,
					const std::function<void(const Drawcall&)>& bind_vertex_func)
	{
		if (draw_list.empty()) return;

		params.command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, pipeline);

		uint32_t prev_node = -1, prev_vertex_buffer = -1, prev_offset = -1, prev_material = -1;

		for (const auto& drawcall : draw_list)
		{
			if (prev_node != drawcall.node_idx)
			{
				auto push_constants = Gbuffer_pipeline::Model_matrix(drawcall.transformation);
				params.command_buffer.push_constants(params.pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constants);
				prev_node = drawcall.node_idx;
			}

			if (drawcall.primitive.material_idx != prev_material)
			{
				params.bind_material_func(drawcall);
				prev_material = drawcall.primitive.material_idx;
			}

			if (drawcall.primitive.position_buffer != prev_vertex_buffer || drawcall.primitive.position_offset != prev_offset)
			{
				bind_vertex_func(drawcall);
				prev_vertex_buffer = drawcall.primitive.position_buffer;
				prev_offset        = drawcall.primitive.position_offset;
			}

			params.command_buffer.draw(0, drawcall.primitive.vertex_count, 0, 1);
		}
	};

	draw(opaque, params.opaque_pipeline, params.bind_vertex_func_opaque);

	draw(mask, params.mask_pipeline, params.bind_vertex_func_mask);

	draw(blend, params.blend_pipeline, params.bind_vertex_func_blend);
}

Drawcall_generator::Gen_result Drawcall_generator::generate(const Gen_params& params)
{
	// check input params
	if (params.model == nullptr) throw Invalid_argument("Invalid input argument: params.model", "params.model", "not nullptr");

	if (params.node_trans_lut == nullptr)
		throw Invalid_argument("Invalid input argument: params.model", "params.node_trans_lut", "not nullptr");

	if (params.scene_idx >= params.model->scenes.size())
		throw Invalid_argument(
			"Invalid argument: params.scene_idx exceeds scene count",
			"params.scene_idx",
			"smaller than scene count"
		);

	// initialize buffers
	single_sided.clear();
	double_sided.clear();
	node_trans_cache.resize(params.model->nodes.size());

	Gen_result result;

	for (auto idx : params.model->scenes[params.scene_idx].nodes) result += generate_node(params, idx, params.base_transformation);

	single_sided.sort();
	double_sided.sort();

	return result;
}

Drawcall_generator::Gen_result Drawcall_generator::generate_node(
	const Gen_params& params,
	uint32_t          node_idx,
	const glm::mat4&  transformation
)
{
	Gen_result result;

	const auto& model = *params.model;
	const auto& node  = model.nodes[node_idx];
	const auto  find  = params.node_trans_lut->find(node_idx);

	const auto node_trans
		= transformation * (find == params.node_trans_lut->end() ? node.transformation.get_mat4() : find->second.get_mat4());
	node_trans_cache[node_idx] = node_trans;

	// recursive render node
	for (auto children_idx : node.children) result += generate_node(params, children_idx, node_trans);

	// ignore nodes without a mesh
	if (!node.has_mesh()) return result;

	const auto& mesh = model.meshes[node.mesh_idx];

	for (const auto& primitive : mesh.primitives)
	{
		// ignore primitives without material
		if (!primitive.has_material()) continue;

		const auto  material_idx = primitive.material_idx;
		const auto& material     = model.materials[material_idx];

		const auto &min = primitive.min, &max = primitive.max;

		/* Construct AABB after transformation */

		auto edge_points = algorithm::geometry::generate_boundaries(min, max);

		glm::vec3 min_coord(std::numeric_limits<float>::max()), max_coord(-std::numeric_limits<float>::max());

		for (auto& pt : edge_points)
		{
			const auto coord = node_trans * glm::vec4(pt, 1.0);
			pt               = coord / coord.w;
			min_coord        = glm::min(min_coord, pt);
			max_coord        = glm::max(max_coord, pt);
		}

		const auto bounding_box = algorithm::geometry::frustum::AABB::from_min_max(min_coord, max_coord);

		const auto edge_bounded
			= bounding_box.intersect_or_forward(params.frustum.bottom) && bounding_box.intersect_or_forward(params.frustum.top)
		   && bounding_box.intersect_or_forward(params.frustum.left) && bounding_box.intersect_or_forward(params.frustum.right);

		// Calculate far & near plane

		float near = std::numeric_limits<float>::max(), far = -near;

		if (edge_bounded)
			for (const auto& pt : edge_points)
			{
				far  = std::max(far, glm::dot(params.eye_path, pt - params.eye_position));
				near = std::min(near, glm::dot(params.eye_path, pt - params.eye_position));
			}

		result.near = std::min(near, result.near);
		result.far  = std::max(far, result.far);

		if (!edge_bounded || !bounding_box.intersect_or_forward(params.frustum.far)
			|| !bounding_box.intersect_or_forward(params.frustum.near))
			continue;

		result.object_count++;
		result.vertex_count += primitive.vertex_count;

		const Drawcall drawcall{node_idx, primitive, node_trans, near, far};

		(material.double_sided ? double_sided : single_sided).emplace(drawcall, material.alpha_mode);
	}

	return result;
}