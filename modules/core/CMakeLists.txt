astral_add_module(NAME core
    SOURCES
        src/version.cpp
        src/log.cpp)

astral_add_tests(MODULE core
    SOURCES
        tests/test_main.cpp
        tests/version_test.cpp
        tests/log_test.cpp)
