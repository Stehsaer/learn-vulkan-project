#pragma once

#include <vulkan/vulkan.hpp>

// #define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#define VKLIB_HPP_NAMESPACE vklib_hpp
