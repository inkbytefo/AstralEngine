astral_add_module(NAME world
    SOURCES
        src/world.cpp
        src/transform.cpp
        src/serialize.cpp
        src/fields.cpp
        src/dynamic.cpp
        src/prefab.cpp
        src/scene.cpp
        src/map.cpp
        src/streaming.cpp
        src/map_state.cpp
        src/level_instance.cpp
        src/origin.cpp
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
        tests/serialize_test.cpp
        tests/dynamic_test.cpp
        tests/scene_test.cpp
        tests/scene_world_crlf_test.cpp
        tests/map_test.cpp
        tests/streaming_test.cpp
        tests/map_state_test.cpp
        tests/origin_test.cpp
        tests/identity_schema_test.cpp
        tests/level_instance_test.cpp)
