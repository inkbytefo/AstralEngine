# VFX showcase: four stations (courtyard VFX timeline, GPU particles + wind + volume flame, gameplay smoke + dispersion,
# atmosphere + froxel fog + sky IBL), with VFX and sound driven from Luau. Replaces vfx_lab.
add_library(vfx_showcase_lib STATIC
    src/game.cpp
    src/stations/stations.cpp
    src/stations/courtyard.cpp
    src/stations/particles.cpp
    src/stations/smoke.cpp
    src/stations/atmosphere.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/vfx_director/vfx_director.cpp)
target_include_directories(vfx_showcase_lib PUBLIC src)
target_link_libraries(vfx_showcase_lib PUBLIC Astral::App)
astral_set_warnings(vfx_showcase_lib)

astral_add_game(vfx_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(vfx_showcase PRIVATE vfx_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(vfx_showcase_tests tests/game_test.cpp)
    target_link_libraries(vfx_showcase_tests PRIVATE vfx_showcase_lib doctest::doctest)
    target_compile_definitions(vfx_showcase_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/vfx_showcase/vfx_showcase.apak")
    add_dependencies(vfx_showcase_tests vfx_showcase_package)
    astral_set_warnings(vfx_showcase_tests)
    add_test(NAME vfx_showcase_tests COMMAND vfx_showcase_tests)
    astral_check_features(vfx_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(vfx_showcase GAME vfx_showcase SCRIPTS assets/features)
endif()
