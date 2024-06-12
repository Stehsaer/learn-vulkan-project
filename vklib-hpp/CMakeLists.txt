project("Vulkan Library Hpp")

if(MSVC)
	# warning level 4
	add_compile_options(/W4)
else()
	# additional warnings
	add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers -fexceptions)
endif()

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(tinyobjloader CONFIG REQUIRED)

# prerequicites
add_library(vklib_hpp STATIC)
aux_source_directory(src vklib_hpp_src)
target_sources(vklib_hpp PRIVATE ${vklib_hpp_src})
target_include_directories(vklib_hpp PUBLIC ./include)
target_compile_features(vklib_hpp PUBLIC cxx_std_20)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(vklib_hpp PUBLIC ${TINYGLTF_INCLUDE_DIRS})

target_link_libraries(vklib_hpp PUBLIC
	Vulkan::Vulkan
	Vulkan::Headers
	GPUOpen::VulkanMemoryAllocator
	tinyobjloader::tinyobjloader
)

target_include_directories(vklib_hpp PUBLIC ${Stb_INCLUDE_DIR})

add_subdirectory(./sdl2)
add_subdirectory(./glfw3)