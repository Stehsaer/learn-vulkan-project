add_requires("stb")

target("vklib_stbi")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("stb", {public = true})
	add_deps("vklib_core", {public = true})