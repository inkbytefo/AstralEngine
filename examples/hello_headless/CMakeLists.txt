# Hello headless: a game without a window or a GPU — features, a Luau rule and a deterministic loop (servers, tools, tests).
add_library(hello_headless_lib STATIC
    src/game.cpp
    src/features/tally/tally.cpp)
target_include_directories(hello_headless_lib PUBLIC src)
target_link_libraries(hello_headless_lib PUBLIC Astral::App)
astral_set_warnings(hello_headless_lib)

astral_add_game(hello_headless SOURCES src/main.cpp ASSETS assets)
target_link_libraries(hello_headless PRIVATE hello_headless_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(hello_headless_tests tests/game_test.cpp)
    target_link_libraries(hello_headless_tests PRIVATE hello_headless_lib doctest::doctest)
    target_compile_definitions(hello_headless_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(hello_headless_tests)
    add_test(NAME hello_headless_tests COMMAND hello_headless_tests)
    astral_check_features(hello_headless SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(hello_headless GAME hello_headless SCRIPTS assets/features)
endif()
