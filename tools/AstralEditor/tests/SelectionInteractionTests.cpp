#include "Astral/Editor/Gizmo/TransformGizmo.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Astral/Core/CommandStack.hpp"
#include "Astral/Editor/Panels/SceneHierarchy.hpp"
#include "Astral/Editor/EditorTheme.hpp"
#include <imgui_internal.h>
#include <stdexcept>
#include <cstring>

namespace Astral {
void RunSelectionInteractionTests() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {800, 700};
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr; int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ApplyAstralTheme(false);
    Scene scene;
    auto make = [&](const char* name) {
        auto e = scene.CreateEntity();
        e.AddComponent<TagComponent>(name);
        e.AddComponent<TransformComponent>();
        return e;
    };
    auto a = make("First"), b = make("Second"), c = make("Third");
    SceneHierarchy hierarchy;
    SelectionContext selection;
    CommandStack commands;
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({340, 650});
        hierarchy.Draw(scene, selection, &commands);
        ImGui::Render();
    };
    for (int i = 0; i < 3; ++i) frame();
    ImGuiWindow* tree = nullptr;
    for (auto* window : GImGui->Windows) if (std::strstr(window->Name, "/EntityTreeChild_")) tree = window;
    if (!tree) throw std::runtime_error("Hierarchy child not found");
    const ImVec2 start = tree->DC.CursorStartPos;
    const float row = ImGui::GetTextLineHeightWithSpacing();
    auto click = [&](int index, bool ctrl) {
        io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
        io.AddMousePosEvent(start.x + 75, start.y + index * row + 7);
        frame();
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame();
    };
    click(0, false); click(1, true); click(2, true);
    if (selection.Entities().size() != 3 || !selection.IsEntitySelected(a) || !selection.IsEntitySelected(b) || !selection.IsEntitySelected(c))
        throw std::runtime_error("Real hierarchy Ctrl+click did not select all three rows");
    click(1, true);
    if (selection.Entities().size() != 2 || selection.IsEntitySelected(b)) throw std::runtime_error("Real hierarchy Ctrl toggle failed");
    click(1, true);
    io.AddKeyEvent(ImGuiKey_D, true); frame();
    io.AddKeyEvent(ImGuiKey_D, false); frame();
    if (selection.Entities().size() != 3 || scene.GetRegistry().GetAliveEntityCount() != 6) throw std::runtime_error("Hierarchy Ctrl+D did not duplicate group");
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddKeyEvent(ImGuiKey_Delete, true); frame();
    io.AddKeyEvent(ImGuiKey_Delete, false); frame();
    if (!selection.Entities().empty() || scene.GetRegistry().GetAliveEntityCount() != 3) throw std::runtime_error("Hierarchy Delete did not remove group");
    commands.Undo();
    if (selection.Entities().size() != 3 || scene.GetRegistry().GetAliveEntityCount() != 6) throw std::runtime_error("Hierarchy group undo failed");
    // Exercise the real ImGuizmo drag path, not only the matrix helper.
    SelectionContext group;
    group.SelectEntity(a); group.SelectEntity(b, true); group.SelectEntity(c, true);
    a.GetComponent<TransformComponent>().position = {-1, 0, 0};
    b.GetComponent<TransformComponent>().position = {1, 0, 0};
    c.GetComponent<TransformComponent>().position = {0, 0, 0};
    TransformGizmo gizmo;
    const glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 6), glm::vec3(0), glm::vec3(0, 1, 0));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    auto gizmoFrame = [&] {
        ImGui::NewFrame(); ImGuizmo::BeginFrame();
        ImGui::SetNextWindowPos({350, 0}); ImGui::SetNextWindowSize({440, 440});
        ImGui::Begin("GizmoTest");
        gizmo.Manipulate(scene, group, view, projection, {360, 20, 400, 400}, nullptr);
        ImGui::End(); ImGui::Render();
    };
    io.AddMousePosEvent(585, 220); gizmoFrame(); gizmoFrame();
    io.AddMouseButtonEvent(0, true); gizmoFrame();
    io.AddMousePosEvent(625, 220); gizmoFrame();
    io.AddMouseButtonEvent(0, false); gizmoFrame();
    const glm::vec3 displacement = c.GetComponent<TransformComponent>().position;
    if (glm::length(displacement) < 0.01f ||
        glm::length(a.GetComponent<TransformComponent>().position - glm::vec3(-1, 0, 0) - displacement) > 0.001f ||
        glm::length(b.GetComponent<TransformComponent>().position - glm::vec3(1, 0, 0) - displacement) > 0.001f)
        throw std::runtime_error("Real ImGuizmo drag did not translate all three selected entities together");
    ImGui::DestroyContext();
}
}



