# Shader showcase (M10, ADR-0022): every surface is a .material file on a .ashader shader; a free camera flies around.
add_library(shader_showcase_lib STATIC
    src/game.cpp
    src/features/fly_camera/fly_camera.cpp)
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
endif()
