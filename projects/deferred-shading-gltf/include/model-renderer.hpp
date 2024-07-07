#pragma once

#include "pipeline.hpp"

struct Drawcall
{
	uint32_t                  node_idx;
	io::mesh::gltf::Primitive primitive;
	glm::mat4                 transformation;

	float near, far;

	bool operator<(const Drawcall& other) const
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

	void emplace(const Drawcall& drawcall, io::mesh::gltf::Alpha_mode mode);

	void sort()
	{
		std::sort(opaque.begin(), opaque.end());
		std::sort(mask.begin(), mask.end());
		std::sort(blend.begin(), blend.end());
	}

	struct Draw_params
	{
		Command_buffer                       command_buffer;
		Graphics_pipeline                    opaque_pipeline;
		Graphics_pipeline                    mask_pipeline;
		Graphics_pipeline                    blend_pipeline;
		Pipeline_layout                      pipeline_layout;
		std::function<void(const Drawcall&)> bind_material_func;
		std::function<void(const Drawcall&)> bind_vertex_func_opaque;
		std::function<void(const Drawcall&)> bind_vertex_func_mask;
		std::function<void(const Drawcall&)> bind_vertex_func_blend;
	};

	void draw(const Draw_params& params) const;
};

class Drawcall_generator
{
  public:

	struct Gen_params
	{
		using Node_lut = std::unordered_map<uint32_t, io::mesh::gltf::Node_transformation>;

		const io::mesh::gltf::Model* model          = nullptr;
		const Node_lut*              node_trans_lut = nullptr;

		algorithm::geometry::frustum::Frustum frustum;
		glm::vec3                             eye_position;
		glm::vec3                             eye_path;
		glm::mat4                             base_transformation = glm::mat4(1.0);

		uint32_t scene_idx;
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

	glm::mat4 get_node_cache(uint32_t idx) const { return idx >= node_trans_cache.size() ? glm::mat4(1.0) : node_trans_cache[idx]; }

	Gen_result generate(const Gen_params& params);

  private:

	Drawlist single_sided, double_sided;

	std::vector<glm::mat4> node_trans_cache;

	Gen_result generate_node(const Gen_params& params, uint32_t node_idx, const glm::mat4& transformation);
};