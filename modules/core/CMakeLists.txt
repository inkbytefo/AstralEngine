configure_file(src/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp @ONLY)

find_package(Threads REQUIRED)

astral_add_module(NAME core
    SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        src/log.cpp
        src/clock.cpp
        src/jobs.cpp
        src/math.cpp
        src/assert.cpp
        src/json.cpp
        src/json_binary.cpp
        src/profiler.cpp
    DEPS
        Threads::Threads)

astral_add_tests(MODULE core
    SOURCES
        tests/test_main.cpp
        tests/version_test.cpp
        tests/log_test.cpp
        tests/handle_test.cpp
        tests/clock_test.cpp
        tests/jobs_test.cpp
        tests/math_test.cpp
        tests/json_test.cpp
        tests/msr_regression_test.cpp
        tests/json_crlf_test.cpp
        tests/reflect_test.cpp
        tests/bytes_test.cpp
        tests/profiler_test.cpp)
if(TARGET astral_core_tests)
    target_compile_definitions(astral_core_tests PRIVATE ASTRAL_EXPECTED_VERSION="${PROJECT_VERSION}")
    # Compile-fail test: a 33-field ASTRAL_REFLECT must stop at a readable static_assert.
    add_executable(astral_core_reflect_too_many_fields EXCLUDE_FROM_ALL tests/compile_fail/reflect_too_many_fields.cpp)
    target_link_libraries(astral_core_reflect_too_many_fields PRIVATE astral_core)
    add_test(NAME astral_core_reflect_too_many_fields
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target astral_core_reflect_too_many_fields)
    set_tests_properties(astral_core_reflect_too_many_fields PROPERTIES
        PASS_REGULAR_EXPRESSION "ASTRAL_REFLECT supports at most 32 fields")
endif()
