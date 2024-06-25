#pragma once

#include "pipeline.hpp"

struct Renderer_drawcall
{
	uint32_t                  node_idx;
	io::mesh::gltf::Primitive primitive;

	auto operator<=>(const Renderer_drawcall& other) const
	{
		return std::tie(primitive.material_idx, primitive.position_buffer, node_idx)
		   <=> std::tie(other.primitive.material_idx, other.primitive.position_buffer, node_idx);
	}
};

class Model_renderer
{
  private:

	std::vector<Renderer_drawcall> single_sided, single_sided_alpha, single_sided_blend, double_sided, double_sided_alpha,
		double_sided_blend;

  public:

	struct Draw_result
	{
		float  near, far;
		double time_consumed;
		uint32_t vertex_count;
	};

	Draw_result render_gltf(
		const Command_buffer&                                        command_buffer,
		const io::mesh::gltf::Model&                                 model,
		const algorithm::frustum_culling::Frustum&                   frustum,
		const glm::vec3&                                             eye_position,
		const glm::vec3&                                             eye_path,
		const Graphics_pipeline&                                     single_pipeline,
		const Graphics_pipeline&                                     single_pipeline_alpha,
		const Graphics_pipeline&                                     single_pipeline_blend,
		const Graphics_pipeline&                                     double_pipeline,
		const Graphics_pipeline&                                     double_pipeline_alpha,
		const Graphics_pipeline&                                     double_pipeline_blend,
		const Pipeline_layout&                                       pipeline_layout,
		const std::function<void(const io::mesh::gltf::Material&)>&  bind_func,
		const std::function<void(const io::mesh::gltf::Primitive&)>& bind_vertex_func_opaque,
		const std::function<void(const io::mesh::gltf::Primitive&)>& bind_vertex_func_mask,
		const std::function<void(const io::mesh::gltf::Primitive&)>& bind_vertex_func_alpha,
		bool                                                         sort_drawcall
	);

	void render_node(
		const io::mesh::gltf::Model&               model,
		uint32_t                                   idx,
		const algorithm::frustum_culling::Frustum& frustum,
		const glm::vec3&                           eye_position,
		const glm::vec3&                           eye_path,
		float&                                     near,
		float&                                     far
	);

	uint32_t get_object_count() const
	{
		return single_sided.size() + double_sided.size() + single_sided_alpha.size() + double_sided_alpha.size();
	}
};
