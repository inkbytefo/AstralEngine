include(FetchContent)
# Dear ImGui, fetched by the render module; built here as Astral::ImGui. Games include <imgui.h>
# to add their own panels in Game::on_debug_ui (engine headers never expose ImGui types).
FetchContent_GetProperties(imgui)
add_library(astral_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/imgui_demo.cpp)
add_library(Astral::ImGui ALIAS astral_imgui)
set_target_properties(astral_imgui PROPERTIES EXPORT_NAME ImGui POSITION_INDEPENDENT_CODE ON)
target_include_directories(astral_imgui SYSTEM PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}> $<INSTALL_INTERFACE:include/imgui>)
target_compile_definitions(astral_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_compile_features(astral_imgui PUBLIC cxx_std_17)
target_compile_options(astral_imgui PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)
if(ASTRAL_INSTALL)
    install(TARGETS astral_imgui EXPORT AstralTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES ${imgui_SOURCE_DIR}/imgui.h ${imgui_SOURCE_DIR}/imconfig.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/imgui)
endif()

astral_add_module(NAME app
    SOURCES
        src/app.cpp
        src/graphics_backend.cpp
        src/services/audio_bridge.cpp
        src/services/atmosphere.cpp
        src/services/debug_overlays.cpp
        src/services/gameplay_smoke.cpp
        src/services/vfx_runtime.cpp
        src/services/dev_tools.cpp
        src/services/frame_loop.cpp
        src/services/physics_bridge.cpp
        src/services/physics_commands.cpp
        src/services/physics_fields.cpp
        src/services/script_physics.cpp
        src/services/render_bridge.cpp
        src/services/render_extract.cpp
        src/services/terrain_bridge.cpp
        src/services/terrain_mesh.cpp
        src/services/script_bridge.cpp
        src/services/system_scheduler.cpp
        src/services/world_bridge.cpp
        src/input_actions.cpp
        src/feature.cpp
        src/feature_check.cpp
        src/shell.cpp
        src/debug_ui.cpp
        src/panels/debug_panel.cpp
        src/panels/demo_panel.cpp
        src/panels/inspector_panel.cpp
        src/panels/json_editor.cpp
        src/panels/materials_panel.cpp
        src/panels/performance_panel.cpp
        src/panels/render_panel.cpp
        src/panels/scene_panel.cpp
        src/panels/world_panel.cpp
        src/map_runtime.cpp
        src/material_library.cpp
    DEPS
        Astral::Core
        Astral::Platform
        Astral::Gpu
        Astral::World
        Astral::Script
        Astral::Render
        Astral::Assets
        Astral::Physics
        Astral::Audio
        Astral::ImGui)

astral_add_tests(MODULE app
    SOURCES
        tests/test_main.cpp
        tests/actions_test.cpp
        tests/actions_crlf_test.cpp
        tests/app_test.cpp
        tests/context_services_test.cpp
        tests/debug_panels_test.cpp
        tests/frame_order_test.cpp
        tests/atmosphere_test.cpp
        tests/gameplay_smoke_test.cpp
        tests/vfx_runtime_test.cpp
        tests/services_test.cpp
        tests/physics_app_test.cpp
        tests/physics_step_contract_test.cpp
        tests/ecs_app_test.cpp
        tests/assets_app_test.cpp
        tests/scene_app_test.cpp
        tests/terrain_app_test.cpp
        tests/pack_app_test.cpp
        tests/devtools_app_test.cpp
        tests/audio_app_test.cpp
        tests/world_app_test.cpp
        tests/script_app_test.cpp
        tests/fields_app_test.cpp
        tests/feature_test.cpp
        tests/shell_test.cpp
        tests/material_app_test.cpp
        tests/refactor_regression_test.cpp)
if(TARGET astral_app_tests)
    FetchContent_GetProperties(stb)
    target_include_directories(astral_app_tests SYSTEM PRIVATE ${stb_SOURCE_DIR})
    set_tests_properties(astral_app_tests PROPERTIES TIMEOUT 300)
endif()
