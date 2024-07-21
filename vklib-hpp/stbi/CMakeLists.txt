find_package(Stb REQUIRED)

add_library(vklib_stbi STATIC)
aux_source_directory(./src vklib_stbi_src)
target_sources(vklib_stbi PRIVATE ${vklib_stbi_src})
target_link_libraries(vklib_stbi PUBLIC vklib_core)
target_include_directories(vklib_stbi PUBLIC ./include)
target_include_directories(vklib_stbi PUBLIC ${Stb_INCLUDE_DIR})
