# Tests that need several modules together (here: a real window presenting through the GPU).
# They skip themselves when there is no display or no Vulkan driver.
add_executable(astral_integration_tests
    ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
    present_test.cpp)
target_link_libraries(astral_integration_tests PRIVATE Astral::Platform Astral::Gpu doctest::doctest)
astral_set_warnings(astral_integration_tests)
add_test(NAME astral_integration_tests COMMAND astral_integration_tests)
set_tests_properties(astral_integration_tests PROPERTIES TIMEOUT 120)
