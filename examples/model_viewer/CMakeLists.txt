# Model viewer: shows a glTF/GLB model in a small lit studio, with hot reload. A static scene: no features.
add_library(model_viewer_lib STATIC src/game.cpp)
target_include_directories(model_viewer_lib PUBLIC src)
target_link_libraries(model_viewer_lib PUBLIC Astral::App)
astral_set_warnings(model_viewer_lib)

# The model comes from the command line, so the game has no assets of its own (the folder only satisfies the packaging helper).
astral_add_game(model_viewer SOURCES src/main.cpp ASSETS assets)
target_link_libraries(model_viewer PRIVATE model_viewer_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(model_viewer_tests tests/game_test.cpp)
    target_link_libraries(model_viewer_tests PRIVATE model_viewer_lib doctest::doctest)
    astral_set_warnings(model_viewer_tests)
    add_test(NAME model_viewer_tests COMMAND model_viewer_tests)
endif()
