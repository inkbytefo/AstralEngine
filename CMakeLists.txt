cmake_minimum_required(VERSION 3.24)
project(Astral VERSION 0.13.0 LANGUAGES CXX)

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
include(AstralEmbed)
include(GNUInstallDirs)

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
add_subdirectory(modules/world)
add_subdirectory(modules/physics)
add_subdirectory(modules/render)
add_subdirectory(modules/audio)
add_subdirectory(modules/assets)
add_subdirectory(modules/app)
add_subdirectory(tools)
include(AstralGame)

if(ASTRAL_BUILD_TESTS)
    add_subdirectory(tests/integration)
    add_test(NAME astral_public_headers_clean
        COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckPublicHeaders.cmake)
endif()

if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(examples)
    if(ASTRAL_BUILD_TESTS)
        add_subdirectory(tests/golden)
    endif()
endif()

if(ASTRAL_INSTALL)
    include(AstralInstall)
endif()
