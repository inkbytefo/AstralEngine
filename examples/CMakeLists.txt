add_executable(hello_core hello_core/main.cpp)
target_link_libraries(hello_core PRIVATE Astral::Core)
astral_set_warnings(hello_core)

add_executable(hello_triangle hello_triangle/main.cpp)
target_link_libraries(hello_triangle PRIVATE Astral::Platform Astral::Gpu)
astral_set_warnings(hello_triangle)
astral_add_shaders(hello_triangle SOURCES hello_triangle/triangle.vert hello_triangle/triangle.frag)

add_executable(spinning_cubes spinning_cubes/main.cpp)
target_link_libraries(spinning_cubes PRIVATE Astral::App)
astral_set_warnings(spinning_cubes)

add_executable(sdf_scene sdf_scene/main.cpp)
target_link_libraries(sdf_scene PRIVATE Astral::App)
astral_set_warnings(sdf_scene)

add_executable(physics_demo physics_demo/main.cpp)
target_link_libraries(physics_demo PRIVATE Astral::App)
astral_set_warnings(physics_demo)

add_executable(rt_showcase rt_showcase/main.cpp)
target_link_libraries(rt_showcase PRIVATE Astral::App)
astral_set_warnings(rt_showcase)

add_executable(model_viewer model_viewer/main.cpp)
target_link_libraries(model_viewer PRIVATE Astral::App)
astral_set_warnings(model_viewer)

# A game in the shape every game takes: exe + assets folder; `scene_viewer_package` cooks and packs.
astral_add_game(scene_viewer SOURCES scene_viewer/main.cpp ASSETS scene_viewer/assets)

# 3D Pong: the smallest complete game on the engine (rules in pong.cpp, everything else in assets/).
astral_add_game(pong SOURCES pong/main.cpp pong/pong.cpp ASSETS pong/assets)
if(ASTRAL_BUILD_TESTS)
    add_executable(pong_tests pong/pong_test.cpp pong/pong.cpp)
    target_link_libraries(pong_tests PRIVATE Astral::App doctest::doctest)
    target_compile_definitions(pong_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/pong/assets")
    astral_set_warnings(pong_tests)
    add_test(NAME pong_tests COMMAND pong_tests)
endif()
