add_library(vklib_sdl2 STATIC)
aux_source_directory(src vklib_hpp_sdl2_src)
target_sources(vklib_sdl2 PRIVATE ${vklib_hpp_sdl2_src})
target_include_directories(vklib_sdl2 PUBLIC ./)
target_link_libraries(vklib_sdl2 PUBLIC vklib_core)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(vklib_sdl2
	PUBLIC
	$<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
	$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
