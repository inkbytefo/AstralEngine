# Spinning cubes: a grid of cubes that spin (C++ parallel system) and bob (Luau), flown over with a free camera.
add_library(spinning_cubes_lib STATIC
    src/game.cpp
    src/features/cubes/cubes.cpp
    src/features/fly_camera/fly_camera.cpp)
target_include_directories(spinning_cubes_lib PUBLIC src)
target_link_libraries(spinning_cubes_lib PUBLIC Astral::App)
astral_set_warnings(spinning_cubes_lib)

astral_add_game(spinning_cubes SOURCES src/main.cpp ASSETS assets)
target_link_libraries(spinning_cubes PRIVATE spinning_cubes_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(spinning_cubes_tests tests/game_test.cpp)
    target_link_libraries(spinning_cubes_tests PRIVATE spinning_cubes_lib doctest::doctest)
    target_compile_definitions(spinning_cubes_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(spinning_cubes_tests)
    add_test(NAME spinning_cubes_tests COMMAND spinning_cubes_tests)
    astral_check_features(spinning_cubes SOURCES src/features MANIFESTS assets/features)
endif()
