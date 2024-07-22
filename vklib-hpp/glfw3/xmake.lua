add_requires("glfw")

target("vklib_glfw")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("glfw", {public = true})
	add_deps("vklib_core", {public = true})