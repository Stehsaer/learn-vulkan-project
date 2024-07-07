cmake_minimum_required(VERSION 3.20.0)

if(${CFG_MINGW})
	message(STATUS "Using x64-mingw")
	set(VCPKG_DEFAULT_HOST_TRIPLET x64-mingw-dynamic)
	set(VCPKG_DEFAULT_TRIPLET x64-mingw-dynamic)
	set(VCPKG_TARGET_TRIPLET "x64-mingw-static")
endif()

project("Vulkan Projects")

find_package(PythonInterp REQUIRED)

message(STATUS "Using Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")

# function of custom target, using glslangValidator to compile
function(compile_shader dependent target variable_name)
	set(shader_src ${CMAKE_CURRENT_SOURCE_DIR}/${target})
	set(shader_dst ${CMAKE_CURRENT_BINARY_DIR}/${target}.spv)
	set(shader_cpp ${CMAKE_CURRENT_BINARY_DIR}/${target}.cpp)
	add_custom_command(
		OUTPUT ${shader_dst}
		COMMAND glslangValidator --target-env vulkan1.1
		-V ${shader_src} -o ${shader_dst}
		DEPENDS ${shader_src}
		COMMENT "Compiling shader ${target}"
	)
	add_custom_command(
		OUTPUT ${shader_cpp}
		COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/convert-bin.py "\"${shader_dst}\"" "\"${shader_cpp}\"" "\"${variable_name}\""
		DEPENDS ${shader_dst}
		COMMENT "Converting binary ${target}"
	)
	target_sources(${dependent} PRIVATE ${shader_cpp})
endfunction()

function(binary_dependency dependent target variable_name)
	set(src ${CMAKE_CURRENT_SOURCE_DIR}/${target})
	set(cpp ${CMAKE_CURRENT_BINARY_DIR}/${target}.cpp)

	add_custom_command(
		OUTPUT ${cpp}
		COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/convert-bin.py "\"${src}\"" "\"${cpp}\"" "\"${variable_name}\""
		DEPENDS ${src}
		COMMENT "Converting binary ${target}"
	)

	target_sources(${dependent} PRIVATE ${cpp})
endfunction()

function(copy_dependency dependent target)
	set(dependency_src ${CMAKE_CURRENT_SOURCE_DIR}/${target})
	set(dependency_dst ${CMAKE_CURRENT_BINARY_DIR}/${target}.res)
	add_custom_command(
		OUTPUT ${dependency_dst}
		COMMAND ${CMAKE_COMMAND} -E copy ${dependency_src} ${dependency_dst}
		DEPENDS ${dependency_src}
		COMMENT "Copying dependency ${target}"
	)
	target_sources(${dependent} PRIVATE ${dependency_dst})
endfunction()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_program(CLANG_TIDY_BIN NAMES "clang-tidy")

if((CMAKE_BUILD_TYPE MATCHES Debug) AND NOT MSVC)
	if(CLANG_TIDY_BIN)
		set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}" "-checks=-*,misc-const-correctness,misc-definitions-in-headers,clang-analyzer-core.NullDereference,clang-analyzer-cplusplus.Move,clang-analyzer-core.NullDereference,clang-analyzer-core.uninitialized.*,misc-throw-by-value-catch-by-reference")
	endif()
endif()

add_subdirectory(vklib-hpp)
add_subdirectory(projects)