# Hello triangle: the smallest picture — one mesh made from raw vertices and one .ashader material. No features.
add_library(hello_triangle_lib STATIC src/game.cpp)
target_include_directories(hello_triangle_lib PUBLIC src)
target_link_libraries(hello_triangle_lib PUBLIC Astral::App)
astral_set_warnings(hello_triangle_lib)

astral_add_game(hello_triangle SOURCES src/main.cpp ASSETS assets)
target_link_libraries(hello_triangle PRIVATE hello_triangle_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(hello_triangle_tests tests/game_test.cpp)
    target_compile_definitions(hello_triangle_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(hello_triangle_tests PRIVATE hello_triangle_lib doctest::doctest)
    astral_set_warnings(hello_triangle_tests)
    add_test(NAME hello_triangle_tests COMMAND hello_triangle_tests)
endif()
