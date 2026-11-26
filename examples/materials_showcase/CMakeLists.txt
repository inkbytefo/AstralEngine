# Materials showcase: three stations, one loaded at a time (advanced PBR studio with decals and car_paint.ashader,
# .material workflow with parent inheritance/triplanar/GLB override/UV inspection, and the .ashader night gallery).
# Replaces material_showcase, material_workflow and shader_showcase.
add_library(materials_showcase_lib STATIC
    src/game.cpp
    src/stations/stations.cpp
    src/stations/studio.cpp
    src/stations/workflow.cpp
    src/stations/gallery.cpp
    src/features/fly_camera/fly_camera.cpp)
target_include_directories(materials_showcase_lib PUBLIC src)
target_link_libraries(materials_showcase_lib PUBLIC Astral::App)
astral_set_warnings(materials_showcase_lib)

astral_add_game(materials_showcase SOURCES src/main.cpp ASSETS assets)
target_link_libraries(materials_showcase PRIVATE materials_showcase_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(materials_showcase_tests tests/game_test.cpp)
    target_link_libraries(materials_showcase_tests PRIVATE materials_showcase_lib doctest::doctest)
    target_compile_definitions(materials_showcase_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_SHOWCASE_TEST_PACK="${CMAKE_BINARY_DIR}/packages/materials_showcase/materials_showcase.apak")
    add_dependencies(materials_showcase_tests materials_showcase_package)
    astral_set_warnings(materials_showcase_tests)
    add_test(NAME materials_showcase_tests COMMAND materials_showcase_tests)
    astral_check_features(materials_showcase SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(materials_showcase GAME materials_showcase SCRIPTS assets/features)
endif()
