include(FetchContent)
# Dear ImGui (MIT): the debug UI (built in the app module). Fetched here because the engine's
# default font (Roboto, Apache 2.0) comes from its misc/fonts folder.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.93.0-docking
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
        src/stores/mesh_store.cpp
        src/stores/texture_store.cpp
        src/stores/material_store.cpp
        src/stores/shader_store.cpp
        src/frame/instances.cpp
        src/passes/shadow_pass.cpp
        src/passes/gbuffer_pass.cpp
        src/passes/sdf_pass.cpp
        src/passes/decal_pass.cpp
        src/passes/lighting_pass.cpp
        src/passes/ddgi_pass.cpp
        src/passes/rt_lighting_pass.cpp
        src/passes/path_tracer_pass.cpp
        src/passes/denoise_pass.cpp
        src/passes/composite_pass.cpp
        src/passes/sky_pass.cpp
        src/passes/aerial_pass.cpp
        src/passes/taa_pass.cpp
        src/passes/bloom_pass.cpp
        src/passes/tonemap_pass.cpp
        src/passes/transparent_pass.cpp
        src/passes/post_pass.cpp
        src/passes/debug_lines_pass.cpp
        src/passes/ui_pass.cpp
        src/paths/raster_path.cpp
        src/paths/ray_traced_path.cpp
        src/render_graph.cpp
        src/debug_draw.cpp
        src/fonts.cpp
        src/text.cpp
        src/surface_shader.cpp
        src/surface_shader_rt.cpp
        src/material_file.cpp
    DEPS
        Astral::Core
        Astral::Gpu
        Astral::Shader)
astral_embed_file(astral_render ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf roboto_medium_ttf)
# Engine shader headers, for .ashader pipelines compiled at run time (ADR-0022).
astral_embed_file(astral_render ${CMAKE_SOURCE_DIR}/modules/gpu/shaders/include/astral/bindless.glsl astral_bindless_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/scene.glsl astral_scene_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/color.glsl astral_color_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/brdf.glsl astral_brdf_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/bsdf.glsl astral_bsdf_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/rt.glsl astral_rt_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/restir.glsl astral_restir_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/primary.glsl astral_primary_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/astral/sdf.glsl astral_sdf_glsl)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/rt_lighting.comp astral_rt_lighting_comp)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ddgi_trace.comp astral_ddgi_trace_comp)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/restir_di_temporal.comp astral_restir_di_temporal_comp)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/restir_di_spatial.comp astral_restir_di_spatial_comp)
astral_embed_file(astral_render ${CMAKE_CURRENT_SOURCE_DIR}/shaders/path_tracer.comp astral_path_tracer_comp)
target_include_directories(astral_render SYSTEM PRIVATE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)

astral_add_shaders(astral_render
    SOURCES
        shaders/gbuffer.vert shaders/gbuffer.frag shaders/shadow.vert
        shaders/sdf.comp shaders/decal.comp shaders/lighting.comp shaders/aerial.comp
        shaders/tonemap.vert shaders/tonemap.frag
        shaders/rt_lighting.comp shaders/ddgi_trace.comp shaders/ddgi_update.comp
        shaders/restir_di_temporal.comp shaders/restir_di_spatial.comp
        shaders/path_tracer.comp
        shaders/denoise_temporal.comp shaders/denoise_atrous.comp shaders/composite.comp
        shaders/taa.comp shaders/bloom_down.comp shaders/bloom_up.comp
        shaders/debug_line.vert shaders/debug_line.frag shaders/ui.vert shaders/ui.frag
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include)

astral_add_tests(MODULE render
    SOURCES
        tests/test_main.cpp
        tests/renderer_test.cpp
        tests/render_audit_test.cpp
        tests/m13_pbr_test.cpp
        tests/stores_test.cpp
        tests/passes_test.cpp
        tests/render_graph_test.cpp
        tests/surface_shader_test.cpp
        tests/material_file_test.cpp
        tests/material_crlf_test.cpp)
if(TARGET astral_render_tests)
    target_include_directories(astral_render_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/gpu/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
