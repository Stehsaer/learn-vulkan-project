add_requires("imgui", {configs = 
	{
		vulkan = true,
		sdl2_no_renderer = true,
		freetype = true
	}
})

target("deferred_shading_gltf")

	-- Basic Properties
	set_kind("binary")
	set_languages("c++20")

	-- Dependencies
	add_deps("vklib_core", "vklib_gltf", "vklib_sdl2", "vklib_stbi")
	add_packages("imgui")

	-- Source Files
    add_includedirs("include")
	add_files("src/*.cpp")
    add_files("src/logic/*.cpp")
	
	-- Shaders
	add_rules("vklib.glsl2spv_opt", {bin2c = true, targetenv = "vulkan1.1"})
	add_files("shaders/*.comp", "shaders/*.frag", "shaders/*.vert")

	-- Binary Resources
	add_files("assets/*", {rule = "utils.bin2c"})
