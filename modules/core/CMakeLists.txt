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
        tests/json_crlf_test.cpp
        tests/reflect_test.cpp
        tests/bytes_test.cpp
        tests/profiler_test.cpp)
if(TARGET astral_core_tests)
    target_compile_definitions(astral_core_tests PRIVATE ASTRAL_EXPECTED_VERSION="${PROJECT_VERSION}")
endif()
