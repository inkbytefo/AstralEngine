cmake_minimum_required(VERSION 3.24)
project(AstralEngine VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Executables land in one folder so samples, editor and tests find each other's assets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ASTRAL_BUILD_EDITOR  "Build the Astral editor"            ON)
option(ASTRAL_BUILD_SAMPLES "Build the sample applications"      ON)
option(ASTRAL_BUILD_TESTS   "Build unit and GPU tests"           ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(AstralTarget)
include(AstralDependencies)
include(AstralShaders)

# Layers: each one only depends on the layers below it.
#   Core   - logging, handles, jobs, events            (no graphics)
#   Render - Vulkan renderer: meshes + SDF, deferred  (depends on Core)
#   Engine - ECS, scenes, assets, physics, input, game loop (depends on Render)
add_subdirectory(modules/Core)
add_subdirectory(modules/Render)
add_subdirectory(modules/Engine)

if(ASTRAL_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(ASTRAL_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
