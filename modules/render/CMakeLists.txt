include(FetchContent)
# Dear ImGui (MIT): the debug UI (built in the app module). Fetched here because the engine's
# default font (Roboto, Apache 2.0) comes from its misc/fonts folder.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
# stb (public domain / MIT): stb_truetype for font rasterising.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

astral_add_module(NAME render
    SOURCES
        src/mesh.cpp
        src/renderer.cpp
        src/render_graph.cpp
        src/debug_draw.cpp
        src/fonts.cpp
        src/text.cpp
        src/surface_shader.cpp
        src/material_file.cpp
    DEPS
        Astral::Core
        Astral::Gpu
        Astral::Shader)
astral_embed_file(astral_render ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf roboto_medium_ttf)
# Engine shader headers, for .ashader pipelines compiled at run time (ADR-0022).
astral_embed_file(astral_render ${CMAKE_SOURCE_DIR}/modules/gpu/shaders/include/astral/bindless.glsl astral_bindless_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/scene.glsl astral_scene_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/rt.glsl astral_rt_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/primary.glsl astral_primary_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/sdf.glsl astral_sdf_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/rt_lighting.comp astral_rt_lighting_comp)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ddgi_trace.comp astral_ddgi_trace_comp)
target_include_directories(astral_render SYSTEM PRIVATE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)

astral_add_shaders(astral_render
    SOURCES
        shaders/gbuffer.vert shaders/gbuffer.frag shaders/shadow.vert
        shaders/sdf.comp shaders/lighting.comp
        shaders/tonemap.vert shaders/tonemap.frag
        shaders/rt_lighting.comp shaders/ddgi_trace.comp shaders/ddgi_update.comp
        shaders/denoise_temporal.comp shaders/denoise_atrous.comp shaders/composite.comp
        shaders/taa.comp shaders/bloom_down.comp shaders/bloom_up.comp
        shaders/debug_line.vert shaders/debug_line.frag shaders/ui.vert shaders/ui.frag
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include)

astral_add_tests(MODULE render
    SOURCES
        tests/test_main.cpp
        tests/renderer_test.cpp
        tests/render_graph_test.cpp
        tests/surface_shader_test.cpp
        tests/material_file_test.cpp)
if(TARGET astral_render_tests)
    target_include_directories(astral_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/gpu/tests)
endif()
