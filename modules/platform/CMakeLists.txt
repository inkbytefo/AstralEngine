find_package(Vulkan 1.3 REQUIRED)
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL ${ASTRAL_INSTALL} CACHE BOOL "" FORCE) # consumers of Astral::Platform link SDL3
set(SDL_RENDER OFF CACHE BOOL "" FORCE)   # we have our own renderer
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.26
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(SDL3)

astral_add_module(NAME platform
    SOURCES
        src/window.cpp
        src/input.cpp
    DEPS
        Astral::Core
        Astral::Gpu
    PRIVATE_DEPS
        SDL3::SDL3-static
        $<BUILD_INTERFACE:Vulkan::Headers>)

astral_add_tests(MODULE platform
    SOURCES
        tests/test_main.cpp
        tests/input_test.cpp
        tests/window_test.cpp)
