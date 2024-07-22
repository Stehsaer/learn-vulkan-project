#include "vklib-core.hpp"

namespace VKLIB_HPP_NAMESPACE::io::wavefront
{
	struct Mesh_vertex
	{
		glm::vec3 position, normal, tangent;
		glm::vec2 uv;
	};

	struct Mesh_data
	{
		std::vector<Mesh_vertex> vertices;
	};

	Mesh_data load_wavefront_obj_mesh(const std::string& file_data);
}