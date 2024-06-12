add_executable(homework0 main.cpp app-init.cpp app-main.cpp)
target_include_directories(homework0 PRIVATE ./)
target_link_libraries(homework0 PRIVATE vklib)
target_link_libraries(homework0 PRIVATE magic_enum::magic_enum)

compile_shader(homework0 shaders/main.vert)
compile_shader(homework0 shaders/main.frag)
copy_dependency(homework0 uv.png)