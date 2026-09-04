#include "Astral/Editor/EditorMenuBar.hpp"
#include "Astral/Core/Components.hpp"

#include <imgui.h>

namespace Astral {

void DrawEditorMenuBar(Scene& scene, Entity& selectedEntity,
                       MenuBarActions& actions, bool& showDemoWindowState) {
    if (!ImGui::BeginMenuBar()) return;

    // ── File ──────────────────────────────────────────────────
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Yeni Sahne", "Ctrl+N")) {
            // Placeholder: sahne sifirlama
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cikis", "Alt+F4")) {
            actions.exitApp = true;
        }
        ImGui::EndMenu();
    }

    // ── Edit ──────────────────────────────────────────────────
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Geri Al", "Ctrl+Z", false, false)) {
            // Placeholder: undo
        }
        if (ImGui::MenuItem("Yeniden Yap", "Ctrl+Y", false, false)) {
            // Placeholder: redo
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Secili Nesneyi Sil", "Delete", false, selectedEntity.IsValid())) {
            actions.deleteSelected = true;
        }
        ImGui::EndMenu();
    }

    // ── Scene ─────────────────────────────────────────────────
    if (ImGui::BeginMenu("Scene")) {
        if (ImGui::BeginMenu("Yeni Nesne Ekle")) {
            if (ImGui::MenuItem("Kure (Sphere)"))   { actions.addSphere = true; }
            if (ImGui::MenuItem("Kutu (Box)"))       { actions.addBox = true; }
            if (ImGui::MenuItem("Torus (Simit)"))    { actions.addTorus = true; }
            if (ImGui::MenuItem("Silindir (Cylinder)")) { actions.addCylinder = true; }
            if (ImGui::MenuItem("Zemin (Plane)"))    { actions.addPlane = true; }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Sahneyi Temizle", nullptr, false, scene.GetRegistry().GetView<TransformComponent>().Size() > 0)) {
            actions.clearScene = true;
        }
        ImGui::EndMenu();
    }

    // ── View ──────────────────────────────────────────────────
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Layout Sifirla (Reset)")) {
            actions.resetLayout = true;
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo Penceresi", nullptr, &showDemoWindowState);
        ImGui::EndMenu();
    }

    // ── Help ──────────────────────────────────────────────────
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Astral Engine Hakkinda")) {
            // Placeholder: about popup
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

} // namespace Astral
