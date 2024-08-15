#pragma once
#include "vklib/core/cmdbuf.hpp"

namespace VKLIB_HPP_NAMESPACE::algorithm
{
	namespace conversion
	{
		// Convert float32 to float16
		uint16_t f32_to_f16(float f32);

		// Convert float32 to float16, output clamped to avoid NaN and Inf
		uint16_t f32_to_f16_clamped(float f32);
	}

	namespace geometry
	{
		// Calculate Tangent in Tangent Space
		glm::vec3 vertex_tangent(
			const glm::vec3& pos0,
			const glm::vec3& pos1,
			const glm::vec3& pos2,
			const glm::vec2& uv0,
			const glm::vec2& uv1,
			const glm::vec2& uv2
		);

		// Generate 8 points for the given AABB Bounding Box
		inline std::array<glm::vec3, 8> generate_boundaries(
			float min_x,
			float max_x,
			float min_y,
			float max_y,
			float min_z,
			float max_z
		)
		{
			return std::to_array<glm::vec3>({
				{min_x, min_y, min_z},
				{min_x, min_y, max_z},
				{min_x, max_y, min_z},
				{min_x, max_y, max_z},
				{max_x, min_y, min_z},
				{max_x, min_y, max_z},
				{max_x, max_y, min_z},
				{max_x, max_y, max_z}
			});
		}

		inline std::array<glm::vec3, 8> generate_boundaries(glm::vec3 min, glm::vec3 max)
		{
			return generate_boundaries(min.x, max.x, min.y, max.y, min.z, max.z);
		}

		namespace frustum
		{
			struct Plane
			{
				glm::vec3 position, normal;

				Plane(const glm::vec3& position, const glm::vec3& normal) :
					position(position),
					normal(glm::normalize(normal))
				{
				}

				Plane() :
					position(0.0),
					normal(0.0)
				{
				}

				float get_signed_distance(const glm::vec3& point) const;
			};

			struct Frustum
			{
				Plane near, far, left, right, top, bottom;

				static Frustum from_projection(
					const glm::vec3& camera_position,
					const glm::vec3& camera_front,
					const glm::vec3& camera_up,
					float            aspect,
					float            fov_y,
					float            near,
					float            far
				);

				static Frustum from_ortho(
					const glm::vec3& camera_position,
					const glm::vec3& camera_front,
					const glm::vec3& camera_up,
					float            left,
					float            right,
					float            bottom,
					float            up,
					float            near,
					float            far
				);
			};

			struct AABB
			{
				glm::vec3 center, extent;

				static AABB from_min_max(const glm::vec3& min, const glm::vec3& max);

				bool intersect_or_forward(const Plane& plane) const;
				bool on_frustum(const Frustum& frustum) const;
				bool inside(const glm::vec3& point) const;
			};
		};
	}

	namespace math
	{
		// Get the convex envelope of given set of 8 points; In-place modification.
		// Returns the actual number of envelope vertices
		size_t get_convex_envelope(std::array<glm::vec3, 8>& input);
	}

	namespace texture
	{
		// Calculate the appropriate mipmap level count, given the width and height of the image.
		// If size smaller than `clamp_size` then returns 1
		inline uint32_t log2_mipmap_level(uint32_t width, uint32_t height, uint32_t clamp_size = 0)
		{
			if (clamp_size != 0 && std ::min(width, height) < clamp_size) return 1;
			return std::floor(std::log2(std::min(width, height)));
		}

		// Generate mipmap from the first level.
		// Contains the most common approach, use with caution if better performance or special need is required.
		void generate_mipmap(
			const Command_buffer& command_buffer,
			const Image&          image,
			uint32_t              width,
			uint32_t              height,
			uint32_t              max_mipmap_level,
			uint32_t              first_layer = 0,
			uint32_t              layer_count = 1,
			vk::ImageLayout       src_layout  = vk::ImageLayout::eTransferSrcOptimal,
			vk::ImageLayout       dst_layout  = vk::ImageLayout::eTransferDstOptimal
		);
	}
}
