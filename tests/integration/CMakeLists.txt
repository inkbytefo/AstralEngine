# Tests that need several modules together (here: a real window presenting through the GPU).
# They skip themselves when there is no display or no Vulkan driver.
add_executable(astral_integration_tests
    ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
    present_test.cpp
    app_window_test.cpp
    fields_gpu_test.cpp)
target_link_libraries(astral_integration_tests PRIVATE Astral::App doctest::doctest)
astral_set_warnings(astral_integration_tests)
add_test(NAME astral_integration_tests COMMAND astral_integration_tests)
set_tests_properties(astral_integration_tests PROPERTIES TIMEOUT 120)
# The shader twin of the physics fields: the test runs fields.glsl on the GPU and compares it with the CPU.
astral_add_shaders(astral_integration_tests SOURCES shaders/fields_probe.comp
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/modules/physics/shaders/include)
