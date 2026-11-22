# Astral::Shader — the .ashader language (ADR-0022): parsing and GLSL -> SPIR-V compiling (glslang,
# private to this module).
include(FetchContent)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL ${ASTRAL_INSTALL} CACHE BOOL "" FORCE)
FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 15.4.0
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(glslang)

astral_add_module(NAME shader
    SOURCES
        src/ashader.cpp
        src/compile.cpp
    DEPS
        Astral::Core
    PRIVATE_DEPS
        glslang::glslang
        glslang::glslang-default-resource-limits)

astral_add_tests(MODULE shader
    SOURCES
        ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
        tests/ashader_test.cpp
        tests/ashader_crlf_test.cpp
        tests/compile_test.cpp)
