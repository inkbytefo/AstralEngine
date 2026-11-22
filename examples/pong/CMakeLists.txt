# 3D Pong: the smallest complete game on the engine, as feature packages (templates/blank shape).
# Rules are Luau (assets/features/pong_rules), two small C++ features add the score lights and the sound.
add_library(pong_game_lib STATIC
    src/game.cpp
    src/features/pong_rules/rules.cpp
    src/features/pong_scorelights/lights.cpp
    src/features/pong_audio/audio.cpp)
target_include_directories(pong_game_lib PUBLIC src)
target_link_libraries(pong_game_lib PUBLIC Astral::App)
astral_set_warnings(pong_game_lib)

astral_add_game(pong SOURCES src/main.cpp ASSETS assets)
target_link_libraries(pong PRIVATE pong_game_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(pong_tests tests/game_test.cpp)
    target_link_libraries(pong_tests PRIVATE pong_game_lib doctest::doctest)
    target_compile_definitions(pong_tests PRIVATE ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(pong_tests)
    add_test(NAME pong_tests COMMAND pong_tests)
    astral_check_features(pong SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(pong GAME pong SCRIPTS assets/features)
endif()
