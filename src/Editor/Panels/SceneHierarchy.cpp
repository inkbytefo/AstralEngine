#include "Astral/Editor/Panels/SceneHierarchy.hpp"
#include "Astral/Core/Components.hpp"

#include <imgui.h>
#include <string>

namespace Astral {

static const char* s_PrimitiveNames[] = {
    "Kure (Sphere)",
    "Kutu (Box)",
    "Torus (Simit)",
    "Silindir (Cylinder)",
    "Zemin (Plane)"
};

void SceneHierarchy::Draw(Scene& scene, Entity& selectedEntity) {
    ImGui::Begin("Sahne Hiyerarsisi");

    // ── Add Entity button (full-width, accent styled) ────────
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.176f, 0.365f, 0.667f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.220f, 0.420f, 0.750f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.145f, 0.310f, 0.580f, 1.0f));

    if (ImGui::Button("+ Yeni Nesne Ekle", ImVec2(-1, 28))) {
        Entity newObj = scene.CreateEntity();
        newObj.AddComponent<TransformComponent>(
            glm::vec3(0.0f, 0.8f, 0.0f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(0.5f)
        );
        newObj.AddComponent<SDFComponent>(
            0u,   // Kure
            3u,   // SmoothUnion
            0.25f, 1u,
            glm::vec3(0.85f, 0.35f, 0.15f),
            0.3f, 0.5f
        );
        selectedEntity = newObj;
    }

    ImGui::PopStyleColor(3);

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2.0f));

    // ── Section label ────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
    ImGui::Text("SAHNE NESNELERI");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2.0f));

    // ── Entity list ──────────────────────────────────────────
    ImVec2 listSize = ImVec2(-1, ImGui::GetContentRegionAvail().y - 40.0f);
    ImGui::BeginChild("EntityList", listSize, true);

    auto& transforms = scene.GetRegistry().GetView<TransformComponent>();

    for (auto&& [entityId, transform] : transforms) {
        Entity currentEntity(entityId, &scene);
        bool isSelected = (selectedEntity == currentEntity);

        // Build display label
        std::string label = currentEntity.ToDisplayString();
        if (currentEntity.HasComponent<SDFComponent>()) {
            const auto& sdf = currentEntity.GetComponent<SDFComponent>();
            uint32_t pIdx = std::min(sdf.primitiveType, 4u);
            label += " (" + std::string(s_PrimitiveNames[pIdx]) + ")";
        }

        // Styling for selected item (Astral accent blue)
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.176f, 0.365f, 0.667f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.176f, 0.365f, 0.667f, 0.7f));
        }

        if (ImGui::Selectable(label.c_str(), isSelected)) {
            selectedEntity = currentEntity;
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Sil")) {
                if (selectedEntity == currentEntity) {
                    selectedEntity = Entity();
                }
                scene.DestroyEntity(currentEntity);
                ImGui::EndPopup();
                if (isSelected) ImGui::PopStyleColor(2);
                break; // Iterator invalidated after destroy
            }
            ImGui::EndPopup();
        }

        if (isSelected) {
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::EndChild();

    // ── Delete selected button ───────────────────────────────
    if (selectedEntity.IsValid()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("Secili Nesneyi Sil", ImVec2(-1, 26))) {
            scene.DestroyEntity(selectedEntity);
            selectedEntity = Entity();
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::End();
}

} // namespace Astral
