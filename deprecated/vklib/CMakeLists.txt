project("Vulkan-library")

if(MSVC)
	# warning level 4
	add_compile_options(/W4)
else()
	# additional warnings
	add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

# prerequicites
add_library(vklib STATIC)
aux_source_directory(src vklib_src)
target_sources(vklib PRIVATE ${vklib_src})
target_include_directories(vklib PUBLIC ./include)
target_compile_features(vklib PUBLIC cxx_std_20)

target_link_libraries(vklib PUBLIC Vulkan::Vulkan)
target_link_libraries(vklib PUBLIC glfw)
target_link_libraries(vklib PUBLIC Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_include_directories(vklib PUBLIC ${Stb_INCLUDE_DIR})
