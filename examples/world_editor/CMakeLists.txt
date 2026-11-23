# Standalone AI-assisted desktop editor example (E0, ADR-0035).
add_executable(world_editor src/main.cpp)
target_link_libraries(world_editor PRIVATE Astral::Editor)
target_compile_definitions(world_editor PRIVATE ASTRAL_EDITOR_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
astral_set_warnings(world_editor)

if(ASTRAL_BUILD_TESTS)
    add_executable(world_editor_tests
        ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
        tests/world_editor_test.cpp
        tests/world_authoring_e1_test.cpp)
    target_link_libraries(world_editor_tests PRIVATE Astral::Editor doctest::doctest)
    target_compile_definitions(world_editor_tests PRIVATE ASTRAL_EDITOR_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    astral_set_warnings(world_editor_tests)
    add_test(NAME world_editor_tests COMMAND world_editor_tests)
endif()
