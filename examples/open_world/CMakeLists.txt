# Open world demo (M9, ADR-0021): fly over a generated 4 x 4 km map whose cells stream in and out around the camera.
# The map is made by tools/make_test_world.py at build time (thousands of entity files stay out of the repository)
# and cooked into streaming cells by astral-cook; the game's own features (fly_camera, day_night) are in assets/features.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(open_world_dir ${CMAKE_CURRENT_BINARY_DIR}/data)
    add_custom_command(
        OUTPUT ${open_world_dir}/source/maps/test.world
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/make_test_world.py ${open_world_dir}/source
        DEPENDS ${CMAKE_SOURCE_DIR}/tools/make_test_world.py
        COMMENT "Generating the open world test map"
        VERBATIM)
    # The game's feature manifests, scripts and actions are cooked together with the map.
    file(GLOB_RECURSE open_world_feature_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*)
    add_custom_command(
        OUTPUT ${open_world_dir}/cooked/maps/test.world
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${open_world_dir}/source
        COMMAND astral_cook ${open_world_dir}/source -o ${open_world_dir}/cooked -q
        DEPENDS ${open_world_dir}/source/maps/test.world ${open_world_feature_files} astral_cook
        COMMENT "Cooking the open world test map"
        VERBATIM)
    add_custom_target(open_world_assets ALL DEPENDS ${open_world_dir}/cooked/maps/test.world)

    add_library(open_world_lib STATIC
        src/game.cpp
        src/features/day_night/day_night.cpp
        src/features/fly_camera/fly_camera.cpp)
    target_include_directories(open_world_lib PUBLIC src)
    target_link_libraries(open_world_lib PUBLIC Astral::App)
    target_compile_definitions(open_world_lib PUBLIC ASTRAL_OPEN_WORLD_ASSETS="${open_world_dir}/cooked")
    astral_set_warnings(open_world_lib)
    add_dependencies(open_world_lib open_world_assets)

    add_executable(open_world src/main.cpp)
    target_link_libraries(open_world PRIVATE open_world_lib)
    astral_set_warnings(open_world)

    if(ASTRAL_BUILD_TESTS)
        add_executable(open_world_tests tests/game_test.cpp)
        target_link_libraries(open_world_tests PRIVATE open_world_lib doctest::doctest)
        astral_set_warnings(open_world_tests)
        add_test(NAME open_world_tests COMMAND open_world_tests)
        astral_check_features(open_world SOURCES src/features MANIFESTS assets/features)
    endif()
endif()
