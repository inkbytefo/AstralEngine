#include "Astral/Editor/Panels/Inspector.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Astral {
namespace {

constexpr const char* PrimitiveNames[] = {
    "Kure (Sphere)", "Kutu (Box)", "Torus", "Zemin (Plane)", "Kapsul (Capsule)", "Silindir (Cylinder)"
};
constexpr const char* OperationNames[] = {
    "Birlestir (Union)", "Cikar (Subtract)", "Kesisim (Intersect)", "Yumusak Birlestir", "Yumusak Cikar"
};

bool DrawVec3Property(const char* label, const char* id, glm::vec3& value, float speed,
                      const glm::vec3& resetValue, float minValue = 0.0f, float maxValue = 0.0f) {
    ImGui::PushID(id);
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    const float resetWidth = ImGui::CalcTextSize("Sifirla").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - resetWidth));
    bool changed = false;
    if (ImGui::SmallButton("Sifirla")) { value = resetValue; changed = true; }
    ImGui::SetNextItemWidth(-1.0f);
    changed |= ImGui::DragFloat3("##Value", glm::value_ptr(value), speed, minValue, maxValue, "%.3f");
    ImGui::PopID();
    return changed;
}

void DrawWorldTransform(Scene& scene, Entity entity) {
    glm::vec3 position, scale;
    glm::quat rotation;
    DecomposeTransformMatrix(scene.GetWorldTransform(entity.GetHandle()), position, rotation, scale);
    const glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
    if (ImGui::BeginTable("WorldTransform", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Alan", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Deger");
        const auto row = [](const char* name, const glm::vec3& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s", name);
            ImGui::TableNextColumn(); ImGui::Text("%.2f  %.2f  %.2f", value.x, value.y, value.z);
        };
        row("Konum", position); row("Rotasyon", euler); row("Olcek", scale);
        ImGui::EndTable();
    }
}

void DrawEmptyState() {
    ImGui::Dummy(ImVec2(0.0f, std::max(20.0f, ImGui::GetContentRegionAvail().y * 0.18f)));
    const char* title = "Nesne secilmedi";
    ImGui::SetCursorPosX(std::max(8.0f, (ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) * 0.5f));
    ImGui::TextDisabled("%s", title);
    ImGui::Spacing();
    ImGui::TextWrapped("Ozellikleri duzenlemek icin Sahne Agaci veya 3D Viewport uzerinden bir nesne secin.");
}

} // namespace

void Inspector::Draw(Scene& scene, Entity& selectedEntity) {
    ImGui::Begin("Bilesen Denetcisi");
    if (!selectedEntity.IsValid()) {
        m_NameEntity = NullEntityHandle;
        DrawEmptyState();
        ImGui::End();
        return;
    }

    if (m_NameEntity != selectedEntity.GetHandle()) {
        m_NameEntity = selectedEntity.GetHandle();
        const std::string name = selectedEntity.HasComponent<TagComponent>()
            ? selectedEntity.GetComponent<TagComponent>().tag : "Entity " + std::to_string(selectedEntity.GetIndex());
        std::snprintf(m_NameBuffer.data(), m_NameBuffer.size(), "%s", name.c_str());
    }

    ImGui::TextDisabled("ENTITY #%u  ·  GEN %u", selectedEntity.GetIndex(), selectedEntity.GetGeneration());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##EntityName", "Nesne adi", m_NameBuffer.data(), m_NameBuffer.size());
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (!selectedEntity.HasComponent<TagComponent>()) selectedEntity.AddComponent<TagComponent>();
        selectedEntity.GetComponent<TagComponent>().tag = m_NameBuffer.data();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("HIYERARSI");
    if (selectedEntity.HasComponent<HierarchyComponent>()) {
        const auto& hierarchy = selectedEntity.GetComponent<HierarchyComponent>();
        const bool hasParent = scene.GetRegistry().IsAlive(hierarchy.parent);
        const std::string parentName = hasParent ? "Entity " + std::to_string(GetEntityIndex(hierarchy.parent)) : "Root";
        ImGui::TextDisabled("Parent"); ImGui::SameLine(88.0f); ImGui::TextUnformatted(parentName.c_str());
        ImGui::TextDisabled("Alt nesneler"); ImGui::SameLine(88.0f); ImGui::Text("%zu", hierarchy.children.size());
        if (hasParent && ImGui::Button("Parent bagini kaldir", ImVec2(-1.0f, 0.0f))) {
            (void)scene.ClearParent(selectedEntity.GetHandle());
        }
    } else {
        ImGui::TextDisabled("Root  ·  Alt nesne yok");
    }

    ImGui::Spacing();
    if (selectedEntity.HasComponent<TransformComponent>() && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& transform = selectedEntity.GetComponent<TransformComponent>();
        ImGui::Indent(6.0f);
        ImGui::TextDisabled("LOCAL");
        DrawVec3Property("Konum", "Position", transform.position, 0.05f, glm::vec3(0.0f));
        glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
        if (DrawVec3Property("Rotasyon", "Rotation", euler, 0.5f, glm::vec3(0.0f))) {
            transform.rotation = glm::normalize(glm::quat(glm::radians(euler)));
        }
        DrawVec3Property("Olcek", "Scale", transform.scale, 0.02f, glm::vec3(1.0f), 0.001f, 1000.0f);
        ImGui::Spacing(); ImGui::TextDisabled("WORLD  ·  SALT OKUNUR");
        DrawWorldTransform(scene, selectedEntity);
        ImGui::Unindent(6.0f);
    }

    if (selectedEntity.HasComponent<SDFComponent>() && ImGui::CollapsingHeader("SDF Geometri", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& sdf = selectedEntity.GetComponent<SDFComponent>();
        ImGui::Indent(6.0f);
        int primitive = static_cast<int>(std::min<uint32_t>(sdf.primitiveType, std::size(PrimitiveNames) - 1));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##Primitive", &primitive, PrimitiveNames, IM_ARRAYSIZE(PrimitiveNames))) sdf.primitiveType = static_cast<uint32_t>(primitive);
        int operation = static_cast<int>(std::min<uint32_t>(sdf.operation, std::size(OperationNames) - 1));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##Operation", &operation, OperationNames, IM_ARRAYSIZE(OperationNames))) sdf.operation = static_cast<uint32_t>(operation);
        ImGui::SetNextItemWidth(-1.0f); ImGui::SliderFloat("##Blend", &sdf.blendFactor, 0.0f, 1.5f, "Blend %.2f m");
        ImGui::Unindent(6.0f);
    }

    if (selectedEntity.HasComponent<SDFComponent>() && ImGui::CollapsingHeader("Materyal")) {
        auto& sdf = selectedEntity.GetComponent<SDFComponent>();
        ImGui::Indent(6.0f);
        ImGui::SetNextItemWidth(-1.0f); ImGui::ColorEdit3("##Albedo", glm::value_ptr(sdf.albedo));
        ImGui::SetNextItemWidth(-1.0f); ImGui::SliderFloat("##Roughness", &sdf.roughness, 0.0f, 1.0f, "Puruzluluk %.2f");
        ImGui::SetNextItemWidth(-1.0f); ImGui::SliderFloat("##Metallic", &sdf.metallic, 0.0f, 1.0f, "Metalik %.2f");
        ImGui::Unindent(6.0f);
    }

    if (selectedEntity.HasComponent<VelocityComponent>() && ImGui::CollapsingHeader("Fizik ve Hiz")) {
        auto& velocity = selectedEntity.GetComponent<VelocityComponent>();
        ImGui::Indent(6.0f);
        DrawVec3Property("Lineer hiz", "LinearVelocity", velocity.linear, 0.1f, glm::vec3(0.0f));
        DrawVec3Property("Acisal hiz", "AngularVelocity", velocity.angular, 0.1f, glm::vec3(0.0f));
        ImGui::Unindent(6.0f);
    }
    ImGui::End();
}

} // namespace Astral
