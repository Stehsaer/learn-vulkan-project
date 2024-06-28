#include "vklib-algorithm.hpp"

#include <memory_resource>
#include <stack>

namespace VKLIB_HPP_NAMESPACE::algorithm
{
	glm::vec3 geometry::vertex_tangent(
		const glm::vec3& pos0,
		const glm::vec3& pos1,
		const glm::vec3& pos2,
		const glm::vec2& uv0,
		const glm::vec2& uv1,
		const glm::vec2& uv2
	)
	{
		// Get Tangent Vector
		// [T B]*[UV1 UV2]=[E1 E2]
		// [T B]=[E1 E2]*inv([UV1 UV2])

		const glm::mat2x3 e_mat(pos1 - pos0, pos2 - pos0);    // [E1 E2]
		const glm::mat2   uv_mat(uv1 - uv0, uv2 - uv0);       // [UV1 UV2]
		const glm::mat2   uv_mat_inv = glm::inverse(uv_mat);  // inv([UV1 UV2])
		const glm::mat2x3 tb_mat     = e_mat * uv_mat_inv;    // [T B]=[E1 E2]*inv([UV1 UV2])
		return glm::normalize(tb_mat[0]);                     // normalize(T)
	}

	namespace geometry::frustum
	{
		float Plane::get_signed_distance(const glm::vec3& point) const
		{
			return glm::dot(normal, point - position);
		}

		// Algorithm from: https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
		Frustum Frustum::from_projection(
			const glm::vec3& camera_position,
			const glm::vec3& camera_front,
			const glm::vec3& camera_up,
			float            aspect,
			float            fov_y,
			float            near,
			float            far
		)
		{
			Frustum f;
			auto    norm_front = glm::normalize(camera_front);
			auto    norm_right = glm::normalize(glm::cross(norm_front, camera_up));
			auto    norm_up    = glm::cross(norm_right, norm_front);

			const float     half_v_side    = far * tanf(fov_y * .5f);
			const float     half_h_side    = half_v_side * aspect;
			const glm::vec3 front_mult_far = far * norm_front;

			f.near = Plane(camera_position + near * norm_front, norm_front);
			f.far  = Plane(camera_position + far * norm_front, -norm_front);

			f.right  = Plane(camera_position, glm::cross(front_mult_far - norm_right * half_h_side, norm_up));
			f.left   = Plane(camera_position, glm::cross(norm_up, front_mult_far + norm_right * half_h_side));
			f.top    = Plane(camera_position, glm::cross(norm_right, front_mult_far - norm_up * half_v_side));
			f.bottom = Plane(camera_position, glm::cross(front_mult_far + norm_up * half_v_side, norm_right));

			return f;
		}

		Frustum Frustum::from_ortho(
			const glm::vec3& camera_position,
			const glm::vec3& camera_front,
			const glm::vec3& camera_up,
			float            left,
			float            right,
			float            bottom,
			float            up,
			float            near,
			float            far
		)
		{
			Frustum f;
			auto    norm_front = glm::normalize(camera_front);
			auto    norm_right = glm::normalize(glm::cross(norm_front, camera_up));
			auto    norm_up    = glm::cross(norm_right, norm_front);

			f.near = Plane(camera_position + near * norm_front, norm_front);
			f.far  = Plane(camera_position + far * norm_front, -norm_front);

			f.left   = Plane(camera_position + norm_right * left, norm_right);
			f.right  = Plane(camera_position + norm_right * right, -norm_right);
			f.bottom = Plane(camera_position + norm_up * bottom, norm_up);
			f.top    = Plane(camera_position + norm_up * up, -norm_up);

			return f;
		}

		AABB AABB::from_min_max(const glm::vec3& min, const glm::vec3& max)
		{
			AABB box;
			box.center = (min + max) / 2.0f;
			box.extent = max - box.center;

			return box;
		}

		bool AABB::intersect_or_forward(const Plane& plane) const
		{
			const float r               = glm::dot(extent, glm::abs(plane.normal));
			const float signed_distance = plane.get_signed_distance(center);

			return signed_distance + r >= 0;
		}

		bool AABB::on_frustum(const Frustum& frustum) const
		{
			return intersect_or_forward(frustum.near) && intersect_or_forward(frustum.far)
				&& intersect_or_forward(frustum.left) && intersect_or_forward(frustum.right)
				&& intersect_or_forward(frustum.top) && intersect_or_forward(frustum.bottom);
		}

		bool AABB::inside(const glm::vec3& point) const
		{
			return abs(point.x - center.x) <= extent.x && abs(point.y - center.y) <= extent.y
				&& abs(point.z - center.z) <= extent.z;
		}
	};

	namespace conversion
	{
		// Created with Github Copilot
		uint16_t f32_to_f16(float f32)
		{
			const uint32_t f32i     = reinterpret_cast<uint32_t&>(f32);
			const uint32_t sign     = (f32i & 0x80000000u) >> 16;
			const uint32_t exponent = (f32i & 0x7F800000u) >> 23;

			uint32_t fraction = f32i & 0x007FFFFFu;
			uint16_t f16      = 0;

			if (exponent == 0)
			{
				// Zero or denormal (treated as zero)
				f16 = 0;
			}
			else if (exponent == 0xFF)
			{
				// Infinity or NaN
				// Convert NaNs to canonical NaN
				if (fraction)
				{
					f16 = 0x7FFF;
				}
				else
				{
					f16 = sign | 0x7C00;
				}
			}
			else
			{
				// Normal number
				int32_t new_exp = static_cast<int32_t>(exponent) - 127;
				if (new_exp < -14)
				{
					// Underflow
					f16 = 0;
				}
				else if (new_exp > 15)
				{
					// Overflow, set to infinity
					f16 = sign | 0x7C00;
				}
				else
				{
					// Representable as a normalized half-precision float
					new_exp += 15;
					fraction >>= 13;
					f16 = sign | (new_exp << 10) | fraction;
				}
			}

			return f16;
		}

		uint16_t f32_to_f16_clamped(float f32)
		{
			const uint32_t f32i     = reinterpret_cast<uint32_t&>(f32);
			const uint32_t sign     = (f32i & 0x80000000u) >> 16;
			const uint32_t exponent = (f32i & 0x7F800000u) >> 23;

			uint32_t fraction = f32i & 0x007FFFFFu;
			uint16_t f16      = 0;

			if (exponent == 0)
			{
				// Zero or denormal (treated as zero)
				f16 = 0;
			}
			else if (exponent == 0xFF)
			{
				// Infinity or NaN
				// Convert NaNs to canonical NaN
				if (fraction)
				{
					f16 = 0x7FFF;
				}
				else
				{
					f16 = sign | 0x7C00;
				}
			}
			else
			{
				// Normal number
				int32_t new_exp = static_cast<int32_t>(exponent) - 127;
				if (new_exp < -14)
				{
					// Underflow
					f16 = 0;
				}
				else if (new_exp > 15)
				{
					// Overflow, set to float16's maximum (65504.0)
					f16 = sign | 0x7BFF;
				}
				else
				{
					// Representable as a normalized half-precision float
					new_exp += 15;
					fraction >>= 13;
					f16 = sign | (new_exp << 10) | fraction;
				}
			}

			return f16;
		}
	}

	namespace texture
	{
		void generate_mipmap(
			const Command_buffer& command_buffer,
			const Image&          image,
			uint32_t              width,
			uint32_t              height,
			uint32_t              max_mipmap_level,
			uint32_t              first_layer,
			uint32_t              layer_count,
			vk::ImageLayout       src_layout,
			vk::ImageLayout       dst_layout
		)
		{
			// No action when max_mipmap_level==0
			if (max_mipmap_level == 0) return;

			int mip_width = width, mip_height = height;

			for (auto i : Range(max_mipmap_level - 1))
			{
				vk::ImageBlit blit_info;

				std::array<vk::Offset3D, 2> src_region, dst_region;

				src_region[0] = vk::Offset3D{0, 0, 0};
				src_region[1] = vk::Offset3D{mip_width, mip_height, 1};
				dst_region[0] = vk::Offset3D{0, 0, 0};
				dst_region[1] = vk::Offset3D{mip_width != 1 ? mip_width / 2 : 1, mip_height != 1 ? mip_height / 2 : 1, 1};

				blit_info.setSrcSubresource({vk::ImageAspectFlagBits::eColor, i, first_layer, layer_count})
					.setDstSubresource({vk::ImageAspectFlagBits::eColor, i + 1, first_layer, layer_count})
					.setSrcOffsets(src_region)
					.setDstOffsets(dst_region);

				// Transit src level layout to SRC
				command_buffer.layout_transit(
					image,
					i == 0 ? src_layout : vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eTransferSrcOptimal,
					vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
					vk::AccessFlagBits::eTransferRead,
					vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eAllGraphics
						| vk::PipelineStageFlagBits::eComputeShader,
					vk::PipelineStageFlagBits::eTransfer,
					{vk::ImageAspectFlagBits::eColor, i, 1, first_layer, layer_count}
				);

				// Transit dst level layout to DST
				command_buffer.layout_transit(
					image,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal,
					vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eShaderWrite
						| vk::AccessFlagBits::eShaderRead,
					vk::AccessFlagBits::eTransferWrite,
					vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eAllGraphics
						| vk::PipelineStageFlagBits::eComputeShader,
					vk::PipelineStageFlagBits::eTransfer,
					{vk::ImageAspectFlagBits::eColor, i + 1, 1, first_layer, layer_count}
				);

				// Do blit
				command_buffer.blit_image(
					image,
					vk::ImageLayout::eTransferSrcOptimal,
					image,
					vk::ImageLayout::eTransferDstOptimal,
					{blit_info},
					vk::Filter::eLinear
				);

				// Transit src level layout to dst_layout
				command_buffer.layout_transit(
					image,
					vk::ImageLayout::eTransferSrcOptimal,
					dst_layout,
					vk::AccessFlagBits::eTransferRead,
					vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead
						| vk::AccessFlagBits::eShaderWrite,
					vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eAllGraphics
						| vk::PipelineStageFlagBits::eComputeShader,
					{vk::ImageAspectFlagBits::eColor, i, 1, first_layer, layer_count}
				);

				mip_width  = dst_region[1].x;
				mip_height = dst_region[1].y;
			}

			// Transit last level to dst_layout
			command_buffer.layout_transit(
				image,
				vk::ImageLayout::eTransferDstOptimal,
				dst_layout,
				vk::AccessFlagBits::eTransferRead,
				vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead
					| vk::AccessFlagBits::eShaderWrite,
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eAllGraphics
					| vk::PipelineStageFlagBits::eComputeShader,
				{vk::ImageAspectFlagBits::eColor, max_mipmap_level - 1, 1, first_layer, layer_count}
			);
		}
	}

	namespace math
	{
		size_t get_convex_envelope(std::array<glm::vec3, 8>& input)
		{
			struct Point
			{
				glm::vec3 pt;
				float     angle = 0;
			};

			std::array<Point, 8> points;

			// avoid allocating on heap
			std::array<uint32_t, 8>             stack_memory_pool;
			std::pmr::monotonic_buffer_resource pool(stack_memory_pool.data(), stack_memory_pool.size() * sizeof(uint32_t));
			std::pmr::vector<uint32_t>          stack(&pool);

			// find smallest y
			for (auto i : Range(1, 8))
				if (input[i].y < input[0].y || (input[i].y == input[0].y && input[i].x < input[0].x)) std::swap(input[i], input[0]);

			// calculate angle
			points[0] = Point{input[0]};
			for (auto i : Range(1, 8))
			{
				const glm::vec2 delta = input[i] - input[0];
				points[i]             = Point{input[i], std::atan2(delta.y, delta.x)};
			}

			// sort
			std::sort(
				points.begin() + 1,
				points.end(),
				[](auto pt1, auto pt2)
				{
					return pt1.angle < pt2.angle;
				}
			);

			stack.push_back(0);
			stack.push_back(1);

			for (auto i : Range(2, 8))
			{
				while (stack.size() >= 2)
				{
					const auto delta1 = points[stack.back()].pt - points[*(stack.rbegin() + 1)].pt;
					const auto delta2 = points[i].pt - points[stack.back()].pt;

					if (delta1.x * delta2.y - delta2.x * delta1.y <= 0)
						stack.pop_back();
					else
						break;
				}

				stack.push_back(i);
			}

			const auto convex_count = stack.size();

			for (auto i : Range(convex_count))
			{
				input[i] = points[stack[i]].pt;
			}

			return convex_count;
		}
	}
}
