#include "data-accessor.hpp"
#include "vklib-gltf.hpp"

namespace VKLIB_HPP_NAMESPACE::io::gltf
{
	Camera::Camera(const tinygltf::Camera& camera) :
		name(camera.name)
	{
		if (camera.type == "perspective")
		{
			is_ortho = false;

			const auto& cam = camera.perspective;

			std::tie(znear, zfar, perspective.yfov, perspective.aspect_ratio)
				= std::tuple(cam.znear, cam.zfar, cam.yfov, cam.aspectRatio);
		}
		else if (camera.type == "orthographic")
		{
			is_ortho = true;

			const auto& cam = camera.orthographic;

			std::tie(znear, zfar, ortho.xmag, ortho.ymag) = std::tuple(cam.znear, cam.zfar, cam.xmag, cam.ymag);
		}
		else
			throw Gltf_spec_violation(
				"Invalid camera mode",
				std::format(R"(Invalid camera mode "{}" found in camera "{}")", camera.type, camera.name),
				"5.12.3. camera.type"
			);
	}
}