find_package(tinyobjloader CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
aux_source_directory(src obj_render_src)

add_executable(obj_render main.cpp)
target_sources(obj_render PRIVATE ${obj_render_src})
target_include_directories(obj_render PUBLIC include)
target_link_libraries(obj_render PUBLIC vklib magic_enum::magic_enum tinyobjloader::tinyobjloader imgui::imgui)

compile_shader(obj_render shaders/main.vert)
compile_shader(obj_render shaders/main.frag)
copy_dependency(obj_render model/model.bin)
copy_dependency(obj_render model/texture.png)
copy_dependency(obj_render fonts/cantarell.otf)
