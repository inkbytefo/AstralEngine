#include "Astral/Editor/EditorWorkspace.hpp"
#include "imgui_internal.h"

namespace Astral {

void SetupDefaultEditorLayout(ImGuiID dockspace_id, bool force) {
    const ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    if (!force && node && (node->IsSplitNode() || !node->Windows.empty())) return;

    // Build before DockSpace(), using the host area below its menu and toolbar.
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodePos(dockspace_id, position);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID left = 0, right = 0, center = 0, remaining = 0;
    ImGuiID viewport = 0, tools = 0;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.19f, &left, &remaining);
    // Relative split: 23% of the full width, leaving approximately 58% centrally.
    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.23f / 0.81f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.32f, &tools, &viewport);
    ImGui::DockBuilderDockWindow("Sahne Hiyerarsisi", left);
    ImGui::DockBuilderDockWindow("Bilesen Denetcisi", right);
    ImGui::DockBuilderDockWindow("3D Viewport", viewport);
    ImGui::DockBuilderDockWindow(EditorToolsWindow, tools);
    // The tools own a TabBar; avoid a redundant docking title strip.
    ImGui::DockBuilderGetNode(tools)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace Astral
