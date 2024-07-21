find_package(Stb REQUIRED)

add_library(vklib_gltf STATIC)
aux_source_directory(./src vklib_gltf_src)
target_sources(vklib_gltf PRIVATE ${vklib_gltf_src})
target_link_libraries(vklib_gltf PUBLIC vklib_core)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(vklib_gltf PUBLIC ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(vklib_gltf PUBLIC ./include)
