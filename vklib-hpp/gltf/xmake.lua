add_requires("tinygltf")

target("vklib_gltf")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("tinygltf", {public = true})
	add_deps("vklib_core", {public = true})