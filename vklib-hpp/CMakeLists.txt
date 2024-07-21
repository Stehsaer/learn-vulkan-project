project("Vulkan Library Hpp")

if(MSVC)
	# warning level 4
	add_compile_options(/W4)
else()
	# additional warnings
	add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers -fexceptions)
endif()

add_subdirectory(./core)
add_subdirectory(./sdl2)
add_subdirectory(./glfw3)
add_subdirectory(./stbi)
add_subdirectory(./gltf)