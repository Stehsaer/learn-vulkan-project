add_library(vklib_glfw3 STATIC)
aux_source_directory(src vklib_hpp_glfw3_src)
target_sources(vklib_glfw3 PRIVATE ${vklib_hpp_glfw3_src})
target_include_directories(vklib_glfw3 PUBLIC ./)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(vklib_glfw3 PUBLIC vklib_core glfw)
