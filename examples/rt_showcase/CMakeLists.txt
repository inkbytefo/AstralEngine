# RT showcase: the ray-traced renderer (ADR-0011) in a closed "RGB room". A static scene: no features, only the room.
add_library(rt_showcase_lib STATIC src/game.cpp src/room.cpp)
target_include_directories(rt_showcase_lib PUBLIC src)
target_link_libraries(rt_showcase_lib PUBLIC Astral::App)
astral_set_warnings(rt_showcase_lib)

astral_add_game(rt_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(rt_showcase PRIVATE rt_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(rt_showcase_tests tests/game_test.cpp)
    target_link_libraries(rt_showcase_tests PRIVATE rt_showcase_lib doctest::doctest)
    astral_set_warnings(rt_showcase_tests)
    add_test(NAME rt_showcase_tests COMMAND rt_showcase_tests)
endif()
