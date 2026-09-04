# AstralEditor exe — ImGui ve ImGuizmo tabanli editor hedefi

add_executable(AstralEditor
    src/main.cpp
    src/EditorUISubsystem.cpp
    src/EditorUI.cpp
    src/EditorTheme.cpp
    src/EditorWorkspace.cpp
    src/EditorMenuBar.cpp
    src/EditorStatusBar.cpp
    src/Gizmo/TransformGizmo.cpp
    src/Gizmo/ViewportGizmoToolbar.cpp
    src/Panels/SceneHierarchy.cpp
    src/Panels/Inspector.cpp
    src/Panels/Statistics.cpp
    src/Panels/ViewportPanel.cpp
    src/Panels/ContentBrowser.cpp
)

target_include_directories(AstralEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(AstralEditor PRIVATE
    AstralEngine
    ImGui
    ImGuizmo
)

set_target_properties(AstralEditor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
