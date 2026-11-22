# Physics demo: rigid bodies, raycasts and a character controller (features: player, shooter, rain, lifetime).
add_library(physics_demo_lib STATIC
    src/game.cpp
    src/features/player/player.cpp
    src/features/shooter/shooter.cpp
    src/features/rain/rain.cpp
    src/features/lifetime/lifetime.cpp)
target_include_directories(physics_demo_lib PUBLIC src)
target_link_libraries(physics_demo_lib PUBLIC Astral::App)
astral_set_warnings(physics_demo_lib)

astral_add_game(physics_demo SOURCES src/main.cpp ASSETS assets)
target_link_libraries(physics_demo PRIVATE physics_demo_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(physics_demo_tests tests/game_test.cpp)
    target_link_libraries(physics_demo_tests PRIVATE physics_demo_lib doctest::doctest)
    target_compile_definitions(physics_demo_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(physics_demo_tests)
    add_test(NAME physics_demo_tests COMMAND physics_demo_tests)
    astral_check_features(physics_demo SOURCES src/features MANIFESTS assets/features)
endif()
