astral_add_module(NAME world
    SOURCES
        src/world.cpp
        src/transform.cpp
        src/serialize.cpp
    DEPS
        Astral::Core)

astral_add_tests(MODULE world
    SOURCES
        tests/test_main.cpp
        tests/world_test.cpp
        tests/transform_test.cpp
        tests/review_test.cpp
        tests/collision_a.cpp
        tests/collision_b.cpp
        tests/query_test.cpp
        tests/serialize_test.cpp)
