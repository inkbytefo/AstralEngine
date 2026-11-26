# World showcase: three stations, one loaded at a time (a 4 x 4 km streamed world with floating origin and a night data
# layer, a 6 x 6 streaming mountain range terrain with height/slope layers, and a 2 x 2 hill terrain for LOD inspection).
# Replaces open_world and terrain_lab.
add_library(world_showcase_lib STATIC
    src/game.cpp
    src/stations/stations.cpp
    src/stations/world_station.cpp
    src/stations/terrain_stations.cpp
    src/features/fly_camera/fly_camera.cpp
    src/features/day_night/day_night.cpp)
target_include_directories(world_showcase_lib PUBLIC src)
target_link_libraries(world_showcase_lib PUBLIC Astral::App)
astral_set_warnings(world_showcase_lib)

astral_add_game(world_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(world_showcase PRIVATE world_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(world_showcase_tests tests/game_test.cpp)
    target_link_libraries(world_showcase_tests PRIVATE world_showcase_lib doctest::doctest)
    target_compile_definitions(world_showcase_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/world_showcase/world_showcase.apak")
    add_dependencies(world_showcase_tests world_showcase_package)
    astral_set_warnings(world_showcase_tests)
    add_test(NAME world_showcase_tests COMMAND world_showcase_tests)
    astral_check_features(world_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(world_showcase GAME world_showcase SCRIPTS assets/features)
endif()
