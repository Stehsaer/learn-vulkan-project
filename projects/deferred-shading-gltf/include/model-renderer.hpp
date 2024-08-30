#pragma once

#include "pipeline.hpp"
#include "render-params.hpp"

class Node_traverser
{
  public:

	struct Traverse_params
	{
		using Node_lut = std::vector<std::optional<io::gltf::Node_transformation>>;

		const io::gltf::Model*       model          = nullptr;
		const Node_lut*              node_trans_lut = nullptr;

		glm::mat4 base_transformation = glm::mat4(1.0);

		uint32_t scene_idx;

		void verify() const;
	};

	struct Traverse_node
	{
		glm::mat4 transform = {1.0};
		bool      traversed = false;
	};

	void traverse(const Traverse_params& params);

	auto operator[](uint32_t idx) const { return transform_list[idx]; }

  private:

	std::vector<Traverse_node> transform_list;

	void traverse(const Traverse_params& params, uint32_t node_idx, const glm::mat4& transform);
};

struct Drawcall
{
	uint32_t                  node_idx;
	io::gltf::Primitive       primitive;
	glm::mat4                 transformation;

	float near, far;

	bool operator<(const Drawcall& other) const;
};

struct Drawlist
{
	std::vector<Drawcall> opaque, mask, blend;

	void clear()
	{
		opaque.clear();
		mask.clear();
		blend.clear();
	}

	size_t size() const { return opaque.size() + mask.size() + blend.size(); }

	void emplace(const Drawcall& drawcall, io::gltf::Alpha_mode mode);

	void sort()
	{
		std::sort(opaque.begin(), opaque.end());
		std::sort(mask.begin(), mask.end());
		std::sort(blend.begin(), blend.end());
	}

	struct Draw_params
	{
		Command_buffer                       command_buffer;
		Model_pipeline_set                   pipeline_set;
		Pipeline_layout                      pipeline_layout;
		std::function<void(const Drawcall&)> bind_material_func;
		std::function<void(const Drawcall&)> bind_vertex_func_opaque;
		std::function<void(const Drawcall&)> bind_vertex_func_mask;
		std::function<void(const Drawcall&)> bind_vertex_func_blend;
		std::function<void(const Drawcall&)> bind_node_func = nullptr;
	};

	void draw(const Draw_params& params) const;
};

class Drawcall_generator
{
  public:

	struct Gen_params
	{
		const io::gltf::Model*       model          = nullptr;
		const Node_traverser*        node_traverser = nullptr;

		algorithm::geometry::frustum::Frustum frustum;
		glm::vec3                             eye_position;
		glm::vec3                             eye_path;

		void set_by_camera_parameter(const Camera_parameter& param)
		{
			frustum      = param.frustum;
			eye_position = param.eye_position;
			eye_path     = param.eye_direction;
		}

		void verify() const;
	};

	struct Gen_result
	{
		// realtime near and far plane
		float near = std::numeric_limits<float>::max(), far = -std::numeric_limits<float>::max();

		// min and max value of bounding box
		glm::vec3 min_bounding{std::numeric_limits<float>::max()}, max_bounding{-std::numeric_limits<float>::max()};

		uint32_t object_count = 0;
		size_t   vertex_count = 0;

		Gen_result& operator+=(const Gen_result& other)
		{
			*this
				= {std::min(near, other.near),
				   std::max(far, other.far),
				   glm::min(min_bounding, other.min_bounding),
				   glm::max(max_bounding, other.max_bounding),
				   object_count + other.object_count,
				   vertex_count + other.vertex_count};

			return *this;
		}
	};

	const Drawlist& get_single_sided_drawlist() const { return single_sided; }
	const Drawlist& get_double_sided_drawlist() const { return double_sided; }
	const Drawlist& get_single_sided_skin_drawlist() const { return single_sided_skin; }
	const Drawlist& get_double_sided_skin_drawlist() const { return double_sided_skin; }

	Gen_result generate(const Gen_params& params);

  private:

	Drawlist single_sided, double_sided, single_sided_skin, double_sided_skin;
};