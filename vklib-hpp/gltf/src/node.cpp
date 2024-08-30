#include "vklib/gltf.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace VKLIB_HPP_NAMESPACE::io::gltf
{
	glm::mat4 Node_transformation::get_mat4() const
	{
		return glm::translate(glm::mat4(1.0), translation) * glm::scale(glm::mat4(rotation), scale);
	}

	void Node_transformation::set(const tinygltf::Node& node)
	{
		// exists full transformation matrix
		if (node.matrix.size() == 16)
		{
			const auto mat = glm::mat4(glm::make_mat4(node.matrix.data()));

			glm::vec3 skew;
			glm::vec4 perspective;

			// decompose the matrix, ignoreing skew and perspective
			glm::decompose(mat, scale, rotation, translation, skew, perspective);

			// conjungatify rotation
			// rotation = glm::conjugate(rotation);

			return;
		}

		if (node.rotation.size() == 4) rotation = glm::make_quat(node.rotation.data());
		if (node.scale.size() == 3) scale = glm::make_vec3(node.scale.data());
		if (node.translation.size() == 3) translation = glm::make_vec3(node.translation.data());
	}

	void Node::set(const tinygltf::Node& node)
	{
		transformation.set(node);

		mesh_idx = to_optional(node.mesh);
		name     = node.name;
		skin_idx = to_optional(node.skin);

		children.resize(node.children.size());
		std::copy(node.children.begin(), node.children.end(), children.begin());
	}
}