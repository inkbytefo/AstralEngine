# Compiles GLSL shaders to SPIR-V and embeds them into the target as a C++ table,
# so applications never have to find shader files on disk.
#
#   astral_embed_shaders(<target> SOURCES <files...> INCLUDE_DIRS <dirs...>)
#
# Stage comes from the extension (.vert, .frag, .comp). Shaders are looked up at
# runtime by file name, e.g. GetEmbeddedShader("Lighting.comp").
find_program(ASTRAL_GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin REQUIRED)

function(astral_embed_shaders target)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDE_DIRS;DEPENDS" ${ARGN})
    set(out_dir ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    set(include_flags "")
    foreach(dir ${ARG_INCLUDE_DIRS})
        list(APPEND include_flags -I${dir})
    endforeach()

    set(spv_files "")
    foreach(src ${ARG_SOURCES})
        get_filename_component(name ${src} NAME)
        set(spv ${out_dir}/${name}.spv)
        add_custom_command(
            OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
            COMMAND ${ASTRAL_GLSLC} --target-env=vulkan1.3 -O ${include_flags} ${src} -o ${spv}
            DEPENDS ${src} ${ARG_DEPENDS}
            COMMENT "glslc ${name}"
            VERBATIM)
        list(APPEND spv_files ${spv})
    endforeach()

    set(generated ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedShaders.cpp)
    add_custom_command(
        OUTPUT ${generated}
        COMMAND ${CMAKE_COMMAND} -DOUTPUT=${generated} "-DINPUTS=${spv_files}"
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EmbedShaders.cmake
        DEPENDS ${spv_files} ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EmbedShaders.cmake
        COMMENT "Embedding ${target} shaders"
        VERBATIM)
    target_sources(${target} PRIVATE ${generated})
endfunction()
