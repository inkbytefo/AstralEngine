# SDF scene: signed-distance-field shapes melting together, orbiting lights, metal spheres (hybrid renderer).
add_library(sdf_scene_lib STATIC
    src/game.cpp
    src/features/orbits/orbits.cpp)
target_include_directories(sdf_scene_lib PUBLIC src)
target_link_libraries(sdf_scene_lib PUBLIC Astral::App)
astral_set_warnings(sdf_scene_lib)

astral_add_game(sdf_scene SOURCES src/main.cpp ASSETS assets)
target_link_libraries(sdf_scene PRIVATE sdf_scene_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(sdf_scene_tests tests/game_test.cpp)
    target_link_libraries(sdf_scene_tests PRIVATE sdf_scene_lib doctest::doctest)
    target_compile_definitions(sdf_scene_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(sdf_scene_tests)
    add_test(NAME sdf_scene_tests COMMAND sdf_scene_tests)
    astral_check_features(sdf_scene SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(sdf_scene GAME sdf_scene SCRIPTS assets/features)
endif()
