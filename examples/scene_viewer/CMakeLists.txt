# Scene viewer: opens a .scene and shows it; scene and prefab files reload while it runs. A viewer: no features.
add_library(scene_viewer_lib STATIC src/game.cpp)
target_include_directories(scene_viewer_lib PUBLIC src)
target_link_libraries(scene_viewer_lib PUBLIC Astral::App)
astral_set_warnings(scene_viewer_lib)

astral_add_game(scene_viewer SOURCES src/main.cpp ASSETS assets)   # `scene_viewer_package` cooks and packs the RGB room
target_link_libraries(scene_viewer PRIVATE scene_viewer_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(scene_viewer_tests tests/game_test.cpp)
    target_link_libraries(scene_viewer_tests PRIVATE scene_viewer_lib doctest::doctest)
    target_compile_definitions(scene_viewer_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(scene_viewer_tests)
    add_test(NAME scene_viewer_tests COMMAND scene_viewer_tests)
endif()
