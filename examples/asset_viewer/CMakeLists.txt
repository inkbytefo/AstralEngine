# Asset viewer: glTF/GLB models and scene files in one viewer, with orbit/fly camera, live stats HUD, hot reload,
# and cook/pack parity. Replaces model_viewer and scene_viewer.
add_library(asset_viewer_lib STATIC
    src/options.cpp
    src/studio.cpp
    src/stats.cpp
    src/game.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/orbit_camera/orbit_camera.cpp)
target_include_directories(asset_viewer_lib PUBLIC src)
target_link_libraries(asset_viewer_lib PUBLIC Astral::App)
astral_set_warnings(asset_viewer_lib)

astral_add_game(asset_viewer SOURCES src/main.cpp ASSETS assets)
target_link_libraries(asset_viewer PRIVATE asset_viewer_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(asset_viewer_tests tests/game_test.cpp)
    target_link_libraries(asset_viewer_tests PRIVATE asset_viewer_lib doctest::doctest)
    target_compile_definitions(asset_viewer_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/asset_viewer/asset_viewer.apak")
    add_dependencies(asset_viewer_tests asset_viewer_package)
    astral_set_warnings(asset_viewer_tests)
    add_test(NAME asset_viewer_tests COMMAND asset_viewer_tests)
    astral_check_features(asset_viewer SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(asset_viewer GAME asset_viewer SCRIPTS assets/features)
endif()
