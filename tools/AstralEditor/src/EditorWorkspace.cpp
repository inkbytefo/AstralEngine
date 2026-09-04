#include "Astral/Editor/EditorWorkspace.hpp"
#include "imgui_internal.h"

namespace Astral {

void SetupDefaultEditorLayout(ImGuiID dockspace_id, bool force) {
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);

    // Only configure if explicitly forced OR if the dock node is empty
    if (!force) {
        if (node != nullptr && (node->ChildNodes[0] != nullptr || node->Windows.Size > 0)) {
            return; // Layout already configured or loaded from imgui.ini
        }
    }

    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // ┌──────────────┬────────────────────┬─────────────────┐
    // │  Outliner     │                    │   Inspector     │
    // │  (Sahne       │   3D Viewport      │   (Transform,   │
    // │  Hiyerarsisi) │    (Merkez)        │    SDF, Mat.)   │
    // │   ~20%        │     ~55%           │    ~25%         │
    // │              ├────────────────────┤                 │
    // │              │ 1. Varlik Tarayici │                 │
    // │              │ 2. Motor Istatistik│                 │
    // └──────────────┴────────────────────┴─────────────────┘

    ImGuiID dock_main = dockspace_id;

    // Split left panel (Scene Hierarchy / Outliner) ~20%
    ImGuiID dock_left;
    ImGuiID dock_remaining;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, &dock_left, &dock_remaining);

    // Split right panel from the remaining ~25% of remaining = ~31% of remaining
    ImGuiID dock_right;
    ImGuiID dock_center;
    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Right, 0.31f, &dock_right, &dock_center);

    // Split center panel into top (3D Viewport) ~65% and bottom (Content Browser + Statistics tab) ~35%
    ImGuiID dock_center_top;
    ImGuiID dock_center_bottom;
    ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.35f, &dock_center_bottom, &dock_center_top);

    // Dock windows into their respective nodes
    ImGui::DockBuilderDockWindow("Sahne Hiyerarsisi", dock_left);
    ImGui::DockBuilderDockWindow("3D Viewport", dock_center_top);
    ImGui::DockBuilderDockWindow("Varlik Tarayicisi (Content Browser)", dock_center_bottom);
    ImGui::DockBuilderDockWindow("Motor Istatistikleri", dock_center_bottom);
    ImGui::DockBuilderDockWindow("Bilesen Denetcisi", dock_right);

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace Astral
