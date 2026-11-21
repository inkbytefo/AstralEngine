astral_add_module(NAME core
    SOURCES
        src/version.cpp
        src/log.cpp
        src/clock.cpp)

astral_add_tests(MODULE core
    SOURCES
        tests/test_main.cpp
        tests/version_test.cpp
        tests/log_test.cpp
        tests/handle_test.cpp
        tests/clock_test.cpp)
