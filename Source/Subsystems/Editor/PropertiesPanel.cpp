#include "PropertiesPanel.h"
#include "../../Subsystems/Scene/Scene.h"
#include "../../Subsystems/Scene/Entity.h"
#include "../../ECS/Components.h"
#include <imgui.h>

namespace AstralEngine {

PropertiesPanel::PropertiesPanel() : EditorPanel("Details") {
}

void PropertiesPanel::OnDraw() {
    if (!m_isOpen) return;

    ImGui::Begin(m_name.c_str(), &m_isOpen);

    if (m_context && m_context->Reg().valid((entt::entity)m_selectedEntity)) {
        DrawComponents(m_selectedEntity);
    } else {
        ImGui::Text("Select an entity to view properties.");
    }

    ImGui::End();
}

void PropertiesPanel::DrawComponents(uint32_t entityID) {
    Entity entity((entt::entity)entityID, m_context.get());

    if (entity.HasComponent<NameComponent>()) {
        auto& name = entity.GetComponent<NameComponent>().name;
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strncpy(buffer, name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
            name = std::string(buffer);
        }
    }

    ImGui::Separator();

    if (entity.HasComponent<TransformComponent>()) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& transform = entity.GetComponent<TransformComponent>();
            ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
            
            glm::vec3 rotationDeg = glm::degrees(transform.rotation);
            if (ImGui::DragFloat3("Rotation", &rotationDeg.x, 0.1f)) {
                transform.rotation = glm::radians(rotationDeg);
            }
            
            ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
        }
    }

    // Add more components here as needed...
}

} // namespace AstralEngine
