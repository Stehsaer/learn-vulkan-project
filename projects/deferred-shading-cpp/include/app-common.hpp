#pragma once

#include <queue>
#include <vklib-sdl2.hpp>
#include <vklib>


using namespace vklib_hpp;

namespace app_image_formats
{
	inline vk::Format shadow_depth_format = vk::Format::eD32Sfloat, main_depth_format = vk::Format::eD32Sfloat,
					  normal_format = vk::Format::eR32G32B32A32Sfloat, color8_format = vk::Format::eR8G8B8A8Unorm,
					  emissive_format = vk::Format::eR16G16B16A16Sfloat;
}
