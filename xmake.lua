add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_requires("glslang", {configs = {binaryonly = true}})
set_languages("c++20")
-- set_toolset("clang")

includes("vklib-hpp")
includes("projects")