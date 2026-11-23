# Material showcase (M13 / ADR-0033): studio gallery demonstrating multi-scattering GGX, clearcoat, sheen,
# anisotropy, Beer-Lambert glass transmission, subsurface scattering, wetness, deferred decals, and Path Tracer mode.
add_library(material_showcase_lib STATIC src/game.cpp src/gallery.cpp)
target_include_directories(material_showcase_lib PUBLIC src)
target_link_libraries(material_showcase_lib PUBLIC Astral::App)
astral_set_warnings(material_showcase_lib)

astral_add_game(material_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(material_showcase PRIVATE material_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(material_showcase_tests tests/game_test.cpp)
    target_link_libraries(material_showcase_tests PRIVATE material_showcase_lib doctest::doctest)
    astral_set_warnings(material_showcase_tests)
    add_test(NAME material_showcase_tests COMMAND material_showcase_tests)
endif()
