astral_add_module(NAME app
    SOURCES
        src/app.cpp
    DEPS
        Astral::Core
        Astral::Platform
        Astral::Gpu
        Astral::World
        Astral::Render
        Astral::Physics)

astral_add_tests(MODULE app
    SOURCES
        tests/test_main.cpp
        tests/app_test.cpp
        tests/physics_app_test.cpp
        tests/ecs_app_test.cpp)
