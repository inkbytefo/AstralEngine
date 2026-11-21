# astral_add_shaders(<target> SOURCES <files...> [INCLUDE_DIRS <dirs...>])
#   Compiles GLSL to SPIR-V with glslc and embeds the result: for "triangle.vert" the target
#   can `#include <shaders/triangle.vert.hpp>` and use `astral::shaders::triangle_vert`
#   (a std::span<const std::uint32_t>). Stage comes from the extension (.vert/.frag/.comp).
find_program(ASTRAL_GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
set(ASTRAL_GPU_SHADER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../modules/gpu/shaders/include
    CACHE INTERNAL "Astral GLSL include directory")

function(astral_add_shaders target)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDE_DIRS" ${ARGN})
    if(NOT ASTRAL_GLSLC)
        message(FATAL_ERROR "glslc not found: install the Vulkan SDK (or shaderc) to build shaders")
    endif()
    set(out_root ${CMAKE_CURRENT_BINARY_DIR}/astral_shaders)
    set(out_dir ${out_root}/shaders)
    file(MAKE_DIRECTORY ${out_dir})
    set(include_flags -I${ASTRAL_GPU_SHADER_INCLUDE_DIR})
    foreach(dir ${ARG_INCLUDE_DIRS})
        list(APPEND include_flags -I${dir})
    endforeach()
    file(GLOB_RECURSE shader_headers ${ASTRAL_GPU_SHADER_INCLUDE_DIR}/*.glsl)

    set(outputs "")
    foreach(src ${ARG_SOURCES})
        get_filename_component(abs ${src} ABSOLUTE)
        get_filename_component(name ${src} NAME)
        string(MAKE_C_IDENTIFIER ${name} ident)
        set(inc ${out_dir}/${name}.inc)
        set(hpp ${out_dir}/${name}.hpp)
        file(WRITE ${hpp}.tmp
            "#pragma once\n// Generated from ${name} by astral_add_shaders. Do not edit.\n"
            "#include <cstdint>\n#include <span>\n\nnamespace astral::shaders {\n"
            "inline constexpr std::uint32_t ${ident}_spirv[] =\n#include \"${name}.inc\"\n;\n"
            "inline constexpr std::span<const std::uint32_t> ${ident}{${ident}_spirv};\n"
            "} // namespace astral::shaders\n")
        configure_file(${hpp}.tmp ${hpp} COPYONLY)
        add_custom_command(
            OUTPUT ${inc}
            COMMAND ${ASTRAL_GLSLC} --target-env=vulkan1.3 -O -mfmt=c ${include_flags} ${abs} -o ${inc}
            DEPENDS ${abs} ${shader_headers}
            COMMENT "glslc ${name}"
            VERBATIM)
        list(APPEND outputs ${inc})
    endforeach()
    add_custom_target(${target}_shaders DEPENDS ${outputs})
    add_dependencies(${target} ${target}_shaders)
    target_include_directories(${target} PRIVATE ${out_root})
endfunction()
