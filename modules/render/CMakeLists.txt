astral_add_module(NAME render
    SOURCES
        src/mesh.cpp
        src/renderer.cpp
        src/render_graph.cpp
    DEPS
        Astral::Core
        Astral::Gpu)
astral_add_shaders(astral_render
    SOURCES
        shaders/gbuffer.vert shaders/gbuffer.frag shaders/shadow.vert
        shaders/sdf.comp shaders/lighting.comp
        shaders/tonemap.vert shaders/tonemap.frag
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include)

astral_add_tests(MODULE render
    SOURCES
        tests/test_main.cpp
        tests/renderer_test.cpp
        tests/render_graph_test.cpp)
if(TARGET astral_render_tests)
    target_include_directories(astral_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/gpu/tests)
endif()
