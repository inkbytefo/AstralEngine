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
        src/debug_ui.cpp
        src/map_runtime.cpp
        src/material_library.cpp
    DEPS
        Astral::Core
        Astral::Platform
        Astral::Gpu
        Astral::World
        Astral::Render
        Astral::Assets
        Astral::Physics
        Astral::Audio
        Astral::ImGui)

astral_add_tests(MODULE app
    SOURCES
        tests/test_main.cpp
        tests/app_test.cpp
        tests/physics_app_test.cpp
        tests/ecs_app_test.cpp
        tests/assets_app_test.cpp
        tests/scene_app_test.cpp
        tests/pack_app_test.cpp
        tests/devtools_app_test.cpp
        tests/audio_app_test.cpp
        tests/world_app_test.cpp
        tests/material_app_test.cpp)
if(TARGET astral_app_tests)
    FetchContent_GetProperties(stb)
    target_include_directories(astral_app_tests SYSTEM PRIVATE ${stb_SOURCE_DIR})
endif()
