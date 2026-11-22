# Blank game on AstralEngine: copy this folder, rename "blank", and start with src/features/ and assets/.
# Builds inside the engine repository (add_subdirectory) and on its own (find_package(Astral)).
cmake_minimum_required(VERSION 3.24)
project(blank_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET Astral::App)
    find_package(Astral 0.16 CONFIG REQUIRED COMPONENTS App)
endif()

# The game's code is a library: the executable and the tests use the same one.
add_library(blank_game_lib STATIC
    src/game.cpp
    src/features/spin/spin.cpp)
target_include_directories(blank_game_lib PUBLIC src)
target_link_libraries(blank_game_lib PUBLIC Astral::App)
if(COMMAND astral_set_warnings)
    astral_set_warnings(blank_game_lib)
endif()

# Executable + `blank_package` (cooks assets/ into blank.apak and stages a runnable folder).
astral_add_game(blank SOURCES src/main.cpp ASSETS assets)
target_link_libraries(blank PRIVATE blank_game_lib)

# Tests: headless, deterministic (tests/game_test.cpp). Feature boundaries are checked too.
include(CTest)
if(BUILD_TESTING AND (NOT DEFINED ASTRAL_BUILD_TESTS OR ASTRAL_BUILD_TESTS))
    if(NOT TARGET doctest::doctest)
        include(FetchContent)
        FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(doctest)
    endif()
    add_executable(blank_tests tests/game_test.cpp)
    target_link_libraries(blank_tests PRIVATE blank_game_lib doctest::doctest)
    target_compile_definitions(blank_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(COMMAND astral_set_warnings)
        astral_set_warnings(blank_tests)
    endif()
    add_test(NAME blank_tests COMMAND blank_tests)
    astral_check_features(blank SOURCES src/features MANIFESTS assets/features)
endif()
