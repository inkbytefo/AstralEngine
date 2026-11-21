cmake_minimum_required(VERSION 3.24)
project(Astral VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTRAL_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AstralWarnings)
include(AstralModule)
include(AstralShaders)

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

# Modules — order follows the dependency layers (lower first).
add_subdirectory(modules/core)
add_subdirectory(modules/gpu)
add_subdirectory(modules/platform)

if(ASTRAL_BUILD_TESTS)
    add_subdirectory(tests/integration)
endif()

if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ASTRAL_INSTALL)
    include(AstralInstall)
endif()
