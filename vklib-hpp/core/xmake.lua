add_requires("vulkansdk", "vulkan-hpp", "vulkan-memory-allocator", "tinyobjloader", "glm")

target("vklib_core")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("vulkansdk", "vulkan-hpp", "vulkan-memory-allocator", "tinyobjloader", "glm", {public = true})