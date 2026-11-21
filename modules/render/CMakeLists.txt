astral_add_module(NAME render
    SOURCES
        src/mesh.cpp
        src/renderer.cpp
        src/render_graph.cpp
        src/debug_draw.cpp
    DEPS
        Astral::Core
        Astral::Gpu)
astral_add_shaders(astral_render
    SOURCES
        shaders/gbuffer.vert shaders/gbuffer.frag shaders/shadow.vert
        shaders/sdf.comp shaders/lighting.comp
        shaders/tonemap.vert shaders/tonemap.frag
        shaders/rt_lighting.comp shaders/ddgi_trace.comp shaders/ddgi_update.comp
        shaders/denoise_temporal.comp shaders/denoise_atrous.comp shaders/composite.comp
        shaders/taa.comp shaders/bloom_down.comp shaders/bloom_up.comp
        shaders/debug_line.vert shaders/debug_line.frag
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
