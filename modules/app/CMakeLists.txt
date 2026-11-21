include(FetchContent)
astral_add_module(NAME app
    SOURCES
        src/app.cpp
    DEPS
        Astral::Core
        Astral::Platform
        Astral::Gpu
        Astral::World
        Astral::Render
        Astral::Assets
        Astral::Physics)

astral_add_tests(MODULE app
    SOURCES
        tests/test_main.cpp
        tests/app_test.cpp
        tests/physics_app_test.cpp
        tests/ecs_app_test.cpp
        tests/assets_app_test.cpp
        tests/scene_app_test.cpp
        tests/pack_app_test.cpp
        tests/devtools_app_test.cpp)
if(TARGET astral_app_tests)
    FetchContent_GetProperties(stb)
    target_include_directories(astral_app_tests SYSTEM PRIVATE ${stb_SOURCE_DIR})
endif()
