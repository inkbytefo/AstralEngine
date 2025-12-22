#include "SceneHierarchyPanel.h"
#include "../../Subsystems/Scene/Scene.h"
#include "../../Subsystems/Scene/Entity.h"
#include "../../ECS/Components.h"
#include "SceneEditorSubsystem.h"
#include "../../Core/Engine.h"

namespace AstralEngine {

SceneHierarchyPanel::SceneHierarchyPanel(std::shared_ptr<Scene> context)
    : EditorPanel("World Outliner"), m_context(context) {
}

void SceneHierarchyPanel::OnDraw() {
    if (!m_isOpen) return;

    ImGui::Begin(m_name.c_str(), &m_isOpen);

    if (m_context) {
        auto view = m_context->Reg().view<IDComponent>();
        for (auto entityID : view) {
            Entity entity(entityID, m_context.get());
            
            bool isRoot = true;
            if (entity.HasComponent<RelationshipComponent>()) {
                if (entity.GetComponent<RelationshipComponent>().Parent != entt::null) {
                    isRoot = false;
                }
            }
            
            if (isRoot) {
                DrawEntityNode((uint32_t)entityID);
            }
        }

        // Context menu on blank space
        if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                m_context->CreateEntity("Empty Entity");
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

void SceneHierarchyPanel::DrawEntityNode(uint32_t entityID) {
    if (!m_context) return;

    Entity entity((entt::entity)entityID, m_context.get());
    const auto& name = entity.GetComponent<NameComponent>().name;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    // Check if it has children
    bool hasChildren = false;
    if (entity.HasComponent<RelationshipComponent>()) {
        hasChildren = !entity.GetComponent<RelationshipComponent>().Children.empty();
    }

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entityID, flags, "%s", name.c_str());

    if (opened && hasChildren) {
        auto& children = entity.GetComponent<RelationshipComponent>().Children;
        for (auto child : children) {
            DrawEntityNode((uint32_t)child);
        }
        ImGui::TreePop();
    }
}

} // namespace AstralEngine
