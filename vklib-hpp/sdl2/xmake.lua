add_requires("libsdl")

target("vklib_sdl2")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("libsdl", {public = true})
	add_deps("vklib_core", {public = true})