add_requires("tinyobjloader")

target("vklib_wavefront")
	set_kind("static")
	set_languages("c++20")
	
	-- Dep
	add_packages("tinyobjloader")
	add_deps("vklib_core")

	-- Files
	add_files("src/*.cpp")
	add_includedirs("include")