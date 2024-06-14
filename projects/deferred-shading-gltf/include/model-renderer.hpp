#pragma once

#include "pipeline.hpp"

struct Renderer_drawcall
{
	uint32_t material_idx, mesh_idx, node_idx;

	auto operator<=>(const Renderer_drawcall& other) const
	{
		return std::tie(material_idx, mesh_idx) <=> std::tie(other.material_idx, other.mesh_idx);
	}
};

class Model_renderer
{
  private:

	std::vector<Renderer_drawcall> single_sided, double_sided;

  public:

	struct Draw_result
	{
		float  near, far;
		double time_consumed;
	};

	Draw_result render_gltf(
		const Command_buffer&                                       command_buffer,
		const io::mesh::gltf::Model&                                model,
		const algorithm::frustum_culling::Frustum&                  frustum,
		const glm::vec3&                                            eye_position,
		const glm::vec3&                                            eye_path,
		const Graphics_pipeline&                                    single_pipeline,
		const Graphics_pipeline&                                    double_pipeline,
		const Pipeline_layout&                                      pipeline_layout,
		const std::function<void(const io::mesh::gltf::Material&)>& bind_func,
		bool                                                        sort_drawcall
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

	uint32_t get_object_count() const { return single_sided.size() + double_sided.size(); }
};
