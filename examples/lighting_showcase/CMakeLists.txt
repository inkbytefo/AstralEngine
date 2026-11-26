# Lighting showcase: three stations, one loaded at a time (a ray-traced RGB room, an SDF yard under the sun, a night
# with many lights). Replaces rt_showcase and sdf_scene.
add_library(lighting_showcase_lib STATIC
    src/game.cpp
    src/stations/stations.cpp
    src/stations/room.cpp
    src/stations/yard.cpp
    src/stations/night.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/orbits/orbits.cpp)
target_include_directories(lighting_showcase_lib PUBLIC src)
target_link_libraries(lighting_showcase_lib PUBLIC Astral::App)
astral_set_warnings(lighting_showcase_lib)

astral_add_game(lighting_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(lighting_showcase PRIVATE lighting_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(lighting_showcase_tests tests/game_test.cpp)
    target_link_libraries(lighting_showcase_tests PRIVATE lighting_showcase_lib doctest::doctest)
    target_compile_definitions(lighting_showcase_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/lighting_showcase/lighting_showcase.apak")
    add_dependencies(lighting_showcase_tests lighting_showcase_package)
    astral_set_warnings(lighting_showcase_tests)
    add_test(NAME lighting_showcase_tests COMMAND lighting_showcase_tests)
    astral_check_features(lighting_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(lighting_showcase GAME lighting_showcase SCRIPTS assets/features)
endif()
