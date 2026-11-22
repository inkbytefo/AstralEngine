include(FetchContent)
# Jolt build switches: portable SIMD (SSE4.2, no AVX), no Jolt-side warnings-as-errors/LTO/FP traps,
# no debug renderer/profiler, and install rules so consumers of Astral::Physics can link it.
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL ${ASTRAL_INSTALL} CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)       # keep RTTI/exceptions consistent with the engine
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.3.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
    SYSTEM)
FetchContent_MakeAvailable(JoltPhysics)

astral_add_module(NAME physics
    SOURCES
        src/physics.cpp
        src/fields.cpp
    DEPS
        Astral::Core
    PRIVATE_DEPS
        Jolt)

if(ASTRAL_INSTALL)
    # fields.glsl: the shader twin of fields.hpp, for games' own shaders (same folder as the gpu module's bindless.glsl).
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/ DESTINATION ${CMAKE_INSTALL_DATADIR}/astral/shaders)
endif()

astral_add_tests(MODULE physics
    SOURCES
        tests/test_main.cpp
        tests/physics_test.cpp
        tests/layers_test.cpp
        tests/shapes_test.cpp
        tests/joints_test.cpp
        tests/fields_test.cpp)
