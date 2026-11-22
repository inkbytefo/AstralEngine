# An authored scene with custom surface, sky and post shaders and a free-flying camera.
add_library(shader_showcase_lib STATIC
    src/game.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/showcase_look/showcase_look.cpp)
target_include_directories(shader_showcase_lib PUBLIC src)
target_link_libraries(shader_showcase_lib PUBLIC Astral::App)
astral_set_warnings(shader_showcase_lib)

astral_add_game(shader_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(shader_showcase PRIVATE shader_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(shader_showcase_tests tests/game_test.cpp)
    target_link_libraries(shader_showcase_tests PRIVATE shader_showcase_lib doctest::doctest)
    target_compile_definitions(shader_showcase_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(shader_showcase_tests)
    add_test(NAME shader_showcase_tests COMMAND shader_showcase_tests)
    astral_check_features(shader_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(shader_showcase GAME shader_showcase SCRIPTS assets/features)
endif()
