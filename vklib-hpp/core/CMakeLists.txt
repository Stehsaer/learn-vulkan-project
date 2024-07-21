find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(tinyobjloader CONFIG REQUIRED)

# prerequicites
add_library(vklib_core STATIC)
aux_source_directory(src vklib_hpp_src)
target_sources(vklib_core PRIVATE ${vklib_hpp_src})
target_include_directories(vklib_core PUBLIC ./include)
target_compile_features(vklib_core PUBLIC cxx_std_20)

target_link_libraries(vklib_core PUBLIC
	Vulkan::Vulkan
	Vulkan::Headers
	GPUOpen::VulkanMemoryAllocator
	tinyobjloader::tinyobjloader
)