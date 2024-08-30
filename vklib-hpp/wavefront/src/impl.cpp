#include "tiny_obj_loader.h"
#include "vklib/wavefront-obj.hpp"

namespace VKLIB_HPP_NAMESPACE::io::wavefront
{
	Mesh_data load_wavefront_obj_mesh(const std::string& file_data)
	{
		// Read
		tinyobj::ObjReader reader;
		{
			tinyobj::ObjReaderConfig reader_config;
			reader_config.triangulate  = true;
			reader_config.vertex_color = false;

			auto parse_success = reader.ParseFromString(file_data, {}, reader_config);
			if (!parse_success)
			{
				throw error::Detailed_error("Failed to parse wavefront obj file", reader.Error());
			}
		}

		// Parse
		const auto& attrib = reader.GetAttrib();
		const auto& shapes = reader.GetShapes();

		Mesh_data output;

		// Iterate over shapes
		for (const auto& shape : shapes)
		{
			const auto& mesh = shape.mesh;

			// Iterate over triangles
			for (size_t face_vert_idx = 0; face_vert_idx < mesh.indices.size(); face_vert_idx += 3)
			{
				std::array<glm::vec3, 3> positions;
				std::array<glm::vec3, 3> normals;
				std::array<glm::vec2, 3> uvs;

				// Regular Data, iterate over 3 vertices in a triangle
				for (auto local_vert_idx : Iota(3))
				{
					const size_t           current_idx      = face_vert_idx + local_vert_idx;
					const tinyobj::index_t current_data_idx = mesh.indices[current_idx];

					positions[local_vert_idx] = *((const glm::vec3*)&attrib.vertices[current_data_idx.vertex_index * 3]);
					normals[local_vert_idx]   = *((const glm::vec3*)&attrib.normals[current_data_idx.normal_index * 3]);
					uvs[local_vert_idx]       = *((const glm::vec2*)&attrib.texcoords[current_data_idx.texcoord_index * 2]);
				}

				const glm::vec3 tangent
					= algorithm::geometry::vertex_tangent(positions[0], positions[1], positions[2], uvs[0], uvs[1], uvs[2]);

				for (auto i : Iota(3))
				{
					output.vertices.emplace_back(positions[i], normals[i], tangent, uvs[i]);
				}
			}
		}

		return output;
	}
}