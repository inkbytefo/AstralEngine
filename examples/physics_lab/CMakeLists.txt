# Physics Lab (P0 — ADR-0038): motorized SixDOF/SwingTwist/Hinge joints, breakable bridge,
# surface material combine modes, cooked shapes, batched queries, streaming detach/attach, and replay determinism.
add_library(physics_lab_lib STATIC
    src/game.cpp
    src/features/lab/lab.cpp)
target_include_directories(physics_lab_lib PUBLIC src)
target_link_libraries(physics_lab_lib PUBLIC Astral::App)
astral_set_warnings(physics_lab_lib)

astral_add_game(physics_lab SOURCES src/main.cpp ASSETS assets)
target_link_libraries(physics_lab PRIVATE physics_lab_lib)

if(ASTRAL_BUILD_TESTS)
    add_executable(physics_lab_tests tests/game_test.cpp)
    target_link_libraries(physics_lab_tests PRIVATE physics_lab_lib doctest::doctest)
    target_compile_definitions(physics_lab_tests PRIVATE
        ASTRAL_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ASTRAL_LAB_TEST_PACK="${CMAKE_BINARY_DIR}/packages/physics_lab/physics_lab.apak")
    add_dependencies(physics_lab_tests physics_lab_package)
    astral_set_warnings(physics_lab_tests)
    add_test(NAME physics_lab_tests COMMAND physics_lab_tests)
    astral_check_features(physics_lab SOURCES src/features MANIFESTS assets/features)
    astral_check_scripts(physics_lab GAME physics_lab SCRIPTS assets/features)
endif()
