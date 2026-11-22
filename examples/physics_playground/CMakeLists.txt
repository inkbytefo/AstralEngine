# Physics playground (M12): a height-field terrain with a lake, floating and sinking bodies, wind, a plank bridge on hinges, a jump pad.
add_library(physics_playground_lib STATIC
    src/game.cpp
    src/features/playground/playground.cpp
    src/features/fly_camera/fly_camera.cpp)
target_include_directories(physics_playground_lib PUBLIC src)
target_link_libraries(physics_playground_lib PUBLIC Astral::App)
astral_set_warnings(physics_playground_lib)

astral_add_game(physics_playground SOURCES src/main.cpp ASSETS assets)
target_link_libraries(physics_playground PRIVATE physics_playground_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(physics_playground_tests tests/game_test.cpp)
    target_link_libraries(physics_playground_tests PRIVATE physics_playground_lib doctest::doctest)
    target_compile_definitions(physics_playground_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(physics_playground_tests)
    add_test(NAME physics_playground_tests COMMAND physics_playground_tests)
    astral_check_features(physics_playground SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(physics_playground GAME physics_playground SCRIPTS assets/features)
endif()
