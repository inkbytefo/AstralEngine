#include "PropertiesPanel.h"
#include "../../ECS/Components.h"
#include "../../Subsystems/Scene/Entity.h"
#include "../../Subsystems/Scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>


static void DrawVec3Control(const std::string &label, glm::vec3 &values,
                            float resetValue = 0.0f,
                            float columnWidth = 100.0f) {
  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, columnWidth);
  ImGui::Text(label.c_str());
  ImGui::NextColumn();

  ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

  float lineHeight =
      GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
  ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  if (ImGui::Button("X", buttonSize))
    values.x = resetValue;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  if (ImGui::Button("Y", buttonSize))
    values.y = resetValue;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  if (ImGui::Button("Z", buttonSize))
    values.z = resetValue;
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();

  ImGui::PopStyleVar();

  ImGui::Columns(1);

  ImGui::PopID();
}

template <typename T, typename UIFunction>
static void DrawComponent(const std::string &name, Entity entity,
                          UIFunction uiFunction) {
  const ImGuiTreeNodeFlags treeNodeFlags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
      ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
      ImGuiTreeNodeFlags_FramePadding;
  if (entity.HasComponent<T>()) {
    auto &component = entity.GetComponent<T>();
    ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
    float lineHeight =
        GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImGui::Separator();
    bool open = ImGui::TreeNodeEx((void *)typeid(T).hash_code(), treeNodeFlags,
                                  name.c_str());
    ImGui::PopStyleVar();

    ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
    if (ImGui::Button("+", ImVec2{lineHeight, lineHeight})) {
      ImGui::OpenPopup("ComponentSettings");
    }

    bool removeComponent = false;
    if (ImGui::BeginPopup("ComponentSettings")) {
      if (ImGui::MenuItem("Remove component"))
        removeComponent = true;

      ImGui::EndPopup();
    }

    if (open) {
      uiFunction(component);
      ImGui::TreePop();
    }

    if (removeComponent)
      entity.RemoveComponent<T>();
  }
}

PropertiesPanel::PropertiesPanel() : EditorPanel("Details") {}

void PropertiesPanel::OnDraw() {
  if (!m_isOpen)
    return;

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
    auto &name = entity.GetComponent<NameComponent>().name;
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy(buffer, name.c_str(), sizeof(buffer));
    if (ImGui::InputText("##Name", buffer, sizeof(buffer))) {
      name = std::string(buffer);
    }
  }

  DrawComponent<TransformComponent>("Transform", entity, [](auto &component) {
    DrawVec3Control("Translation", component.position);
    glm::vec3 rotation = glm::degrees(component.rotation);
    DrawVec3Control("Rotation", rotation);
    component.rotation = glm::radians(rotation);
    DrawVec3Control("Scale", component.scale, 1.0f);
  });

  DrawComponent<RenderComponent>("Mesh Renderer", entity, [](auto &component) {
    ImGui::Checkbox("Visible", &component.visible);
    ImGui::Checkbox("Cast Shadows", &component.castsShadows);
    ImGui::Checkbox("Receive Shadows", &component.receivesShadows);

    ImGui::Text("Model: %llu", component.modelHandle.GetID());
    ImGui::Text("Material: %llu", component.materialHandle.GetID());
  });

  DrawComponent<LightComponent>("Light", entity, [](auto &component) {
    const char *lightTypes[] = {"Directional", "Point", "Spot"};
    int currentType = (int)component.type;
    if (ImGui::Combo("Type", &currentType, lightTypes, 3)) {
      component.type = (LightComponent::LightType)currentType;
    }

    ImGui::ColorEdit3("Color", &component.color.r);
    ImGui::DragFloat("Intensity", &component.intensity, 0.1f, 0.0f, 100.0f);

    if (component.type != LightComponent::LightType::Directional) {
      ImGui::DragFloat("Range", &component.range, 0.1f, 0.0f, 1000.0f);
    }

    if (component.type == LightComponent::LightType::Spot) {
      ImGui::DragFloat("Inner Angle", &component.innerConeAngle, 0.1f, 0.0f,
                       component.outerConeAngle);
      ImGui::DragFloat("Outer Angle", &component.outerConeAngle, 0.1f,
                       component.innerConeAngle, 180.0f);
    }
  });

  DrawComponent<CameraComponent>("Camera", entity, [](auto &component) {
    const char *projTypes[] = {"Perspective", "Orthographic"};
    int currentProj = (int)component.projectionType;
    if (ImGui::Combo("Projection", &currentProj, projTypes, 2)) {
      component.projectionType = (CameraComponent::ProjectionType)currentProj;
    }

    if (component.projectionType ==
        CameraComponent::ProjectionType::Perspective) {
      ImGui::DragFloat("FOV", &component.fieldOfView, 0.1f, 1.0f, 120.0f);
    }

    ImGui::DragFloat("Near", &component.nearPlane, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far", &component.farPlane, 1.0f, 10.0f, 10000.0f);
    ImGui::Checkbox("Main Camera", &component.isMainCamera);
  });
}

} // namespace AstralEngine
