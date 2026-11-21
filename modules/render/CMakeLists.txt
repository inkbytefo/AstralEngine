astral_add_module(NAME render
    SOURCES
        src/mesh.cpp
        src/renderer.cpp
    DEPS
        Astral::Core
        Astral::Gpu)
astral_add_shaders(astral_render SOURCES shaders/mesh.vert shaders/mesh.frag)

astral_add_tests(MODULE render
    SOURCES
        tests/test_main.cpp
        tests/renderer_test.cpp)
