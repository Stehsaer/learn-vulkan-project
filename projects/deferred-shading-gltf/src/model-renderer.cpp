#include "model-renderer.hpp"

#pragma region /* Node_traverser::Traverse_params */

void Node_traverser::Traverse_params::verify() const
{
	error::Invalid_argument::check(model != nullptr, "params.model should be non-NULL");
	error::Invalid_argument::check(node_trans_lut != nullptr, "params.node_trans_lut should be non-NULL");
	error::Invalid_argument::check(scene_idx < model->scenes.size(), "params.scene_idx should be smaller than scene count");
}

void Node_traverser::traverse(const Traverse_params& params)
{
	const auto& model = *params.model;

	transform_list.resize(model.nodes.size(), {});
	const auto& scene = model.scenes[params.scene_idx];

	for (const auto node_idx : scene.nodes)
	{
		traverse(params, node_idx, params.base_transformation);
	}
}

void Node_traverser::traverse(const Traverse_params& params, uint32_t node_idx, const glm::mat4& transform)
{
	const auto& model = *params.model;
	const auto& node  = model.nodes[node_idx];
	const auto& find  = (*params.node_trans_lut)[node_idx];

	const auto node_trans = transform * (find == std::nullopt ? node.transformation.get_mat4() : find.value().get_mat4());

	transform_list[node_idx] = {node_trans, true};

	for (const auto idx : node.children) traverse(params, idx, node_trans);
}

#pragma endregion

#pragma region /* Drawcall */

bool Drawcall::operator<(const Drawcall& other) const
{
	struct Sort_distance
	{
		float near, far;

		Sort_distance(const Drawcall& drawcall) :
			near(drawcall.near),
			far(drawcall.far)
		{
		}

		Sort_distance() = default;

		auto operator<=>(const Sort_distance& other) const
		{
			if (near < 0 && other.near < 0) return far <=> other.far;

			if (near < 0) return std::partial_ordering::less;
			if (other.near < 0) return std::partial_ordering::greater;

			return std::tuple(far, near) <=> std::tuple(other.far, other.near);
		}
	};

	return std::tuple(Sort_distance(*this), primitive.material_idx, primitive.position_buffer, node_idx)
		 < std::tuple(Sort_distance(other), other.primitive.material_idx, other.primitive.position_buffer, node_idx);
}

#pragma endregion

#pragma region /* Drawlist */

void Drawlist::emplace(const Drawcall& drawcall, io::gltf::Alpha_mode mode)
{
	switch (mode)
	{
	case io::gltf::Alpha_mode::Opaque:
		opaque.emplace_back(drawcall);
		break;
	case io::gltf::Alpha_mode::Mask:
		mask.emplace_back(drawcall);
		break;
	case io::gltf::Alpha_mode::Blend:
		blend.emplace_back(drawcall);
		break;
	}
}

void Drawlist::draw(const Draw_params& params) const
{
	auto draw = [&](const std::vector<Drawcall>&                draw_list,
					const Graphics_pipeline&                    pipeline,
					const std::function<void(const Drawcall&)>& bind_vertex_func,
					const std::function<void(const Drawcall&)>& bind_node_func)
	{
		if (draw_list.empty()) return;

		params.command_buffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, pipeline);

		uint32_t                               prev_node = -1, prev_vertex_buffer = -1, prev_offset = -1;
		std::optional<std::optional<uint32_t>> prev_material = std::nullopt;

		for (const auto& drawcall : draw_list)
		{
			if (prev_node != drawcall.node_idx)
			{
				auto push_constants = Gbuffer_pipeline::Model_matrix(drawcall.transformation);
				params.command_buffer.push_constants(params.pipeline_layout, vk::ShaderStageFlagBits::eVertex, push_constants);

				if (bind_node_func != nullptr) bind_node_func(drawcall);

				prev_node = drawcall.node_idx;
			}

			if (!prev_material.has_value() || drawcall.primitive.material_idx != prev_material.value())
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

	draw(opaque, params.pipeline_set.opaque, params.bind_vertex_func_opaque, params.bind_node_func);

	draw(mask, params.pipeline_set.mask, params.bind_vertex_func_mask, params.bind_node_func);

	draw(blend, params.pipeline_set.blend, params.bind_vertex_func_blend, params.bind_node_func);
}

#pragma endregion

#pragma region /* Drawcall_generator::Gen_params */

void Drawcall_generator::Gen_params::verify() const
{
	error::Invalid_argument::check(model != nullptr, "params.model should be non-NULL");
	error::Invalid_argument::check(node_traverser != nullptr, "params.node_traverser should be non-NULL");
}

#pragma endregion

#pragma region /* Drawcall_generator */

Drawcall_generator::Gen_result Drawcall_generator::generate(const Gen_params& params)
{
	// Verify input parameters
	params.verify();

	// initialize buffers
	single_sided.clear();
	double_sided.clear();
	single_sided_skin.clear();
	double_sided_skin.clear();

	Gen_result result;
	const auto& model     = *params.model;
	const auto& traverser = *params.node_traverser;

	// Iterates over all nodes
	for (auto [node_idx, node] : Walk(model.nodes))
	{
		const auto node_trans = traverser[node_idx].transform;

		// Skip nodes without mesh
		if (!node.mesh_idx || !traverser[node_idx].traversed) continue;

		const auto& mesh = model.meshes[node.mesh_idx.value()];

		for (const auto& primitive : mesh.primitives)
		{
			// skip primitives without material

			const auto  material_idx = primitive.material_idx;
			const auto& material     = primitive.material_idx ? model.materials[material_idx.value()] : model.materials.back();

			const auto &min = primitive.min, &max = primitive.max;

			/* Construct AABB after transformation */

			glm::vec3 min_coord(std::numeric_limits<float>::max()), max_coord(-min_coord);
			float     near = std::numeric_limits<float>::max(), far = -near;

			auto edge_points = algorithm::geometry::generate_boundaries(min, max);

			if (node.skin_idx && primitive.skin)
			{
				const auto& skin = model.skins[node.skin_idx.value()];

				// iterates over all joints, and get an oversized bounding box
				for (auto [i, joint_idx] : Walk(skin.joints))
				{
					auto       local_edge_points = edge_points;

					for (auto& pt : local_edge_points)
					{
						const auto coord = traverser[joint_idx].transform * skin.inverse_bind_matrices[i] * glm::vec4(pt, 1.0);
						pt               = coord / coord.w;
						min_coord        = glm::min(min_coord, pt);
						max_coord        = glm::max(max_coord, pt);
					}
				}

				edge_points = algorithm::geometry::generate_boundaries(min_coord, max_coord);
			}
			else
			{
				for (auto& pt : edge_points)
				{
					const auto coord = node_trans * glm::vec4(pt, 1.0);
					pt               = coord / coord.w;
					min_coord        = glm::min(min_coord, pt);
					max_coord        = glm::max(max_coord, pt);
				}
			}

			const auto bounding_box = algorithm::geometry::frustum::AABB::from_min_max(min_coord, max_coord);

			const auto edge_bounded
				= bounding_box.intersect_or_forward(params.frustum.bottom) && bounding_box.intersect_or_forward(params.frustum.top)
			   && bounding_box.intersect_or_forward(params.frustum.left) && bounding_box.intersect_or_forward(params.frustum.right);

			// Calculate far & near plane
			if (edge_bounded)
				for (const auto& pt : edge_points)
				{
					far  = std::max(far, glm::dot(params.eye_path, pt - params.eye_position));
					near = std::min(near, glm::dot(params.eye_path, pt - params.eye_position));
				}

			result.near         = std::min(near, result.near);
			result.far          = std::max(far, result.far);
			result.min_bounding = glm::min(min_coord, result.min_bounding);
			result.max_bounding = glm::max(max_coord, result.max_bounding);

			if (!edge_bounded || !bounding_box.intersect_or_forward(params.frustum.far)
				|| !bounding_box.intersect_or_forward(params.frustum.near))
				continue;

			result.object_count++;
			result.vertex_count += primitive.vertex_count;

			const Drawcall drawcall{(uint32_t)node_idx, primitive, node_trans, near, far};

			auto& non_skin_side = material.double_sided ? double_sided : single_sided;
			auto& skin_side     = material.double_sided ? double_sided_skin : single_sided_skin;

			(primitive.skin ? skin_side : non_skin_side).emplace(drawcall, material.alpha_mode);
		}
	}

	single_sided.sort();
	double_sided.sort();

	return result;
}

#pragma endregion