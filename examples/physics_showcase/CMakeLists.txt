# Physics showcase: one world, four stations around a lake (pile, joints, surfaces, water and wind). Replaces
# physics_demo, physics_lab and physics_playground.
add_library(physics_showcase_lib STATIC
    src/game.cpp
    src/stations/terrain.cpp
    src/stations/pile.cpp
    src/stations/joints.cpp
    src/stations/surfaces.cpp
    src/stations/water.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/lifetime/lifetime.cpp
    src/features/player/player.cpp
    src/features/shooter/shooter.cpp
    src/features/rain/rain.cpp
    src/features/lab/lab.cpp
    src/features/playground/playground.cpp
    src/features/showcase/showcase.cpp)
target_include_directories(physics_showcase_lib PUBLIC src)
target_link_libraries(physics_showcase_lib PUBLIC Astral::App)
astral_set_warnings(physics_showcase_lib)

astral_add_game(physics_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(physics_showcase PRIVATE physics_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(physics_showcase_tests tests/game_test.cpp)
    target_link_libraries(physics_showcase_tests PRIVATE physics_showcase_lib doctest::doctest)
    target_compile_definitions(physics_showcase_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/physics_showcase/physics_showcase.apak")
    add_dependencies(physics_showcase_tests physics_showcase_package)
    astral_set_warnings(physics_showcase_tests)
    add_test(NAME physics_showcase_tests COMMAND physics_showcase_tests)
    astral_check_features(physics_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(physics_showcase GAME physics_showcase SCRIPTS assets/features)
endif()
