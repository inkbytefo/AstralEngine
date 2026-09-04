#include "Astral/Editor/Panels/Inspector.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Astral {

static const char* s_PrimitiveNames[] = {
    "Kure (Sphere)",
    "Kutu (Box)",
    "Torus (Simit)",
    "Zemin (Plane)",
    "Kapsul (Capsule)",
    "Silindir (Cylinder)"
};

static const char* s_OperationNames[] = {
    "Birlestir (Union)",
    "Cikar (Subtract)",
    "Kesisim (Intersect)",
    "Yumusak Birlestir (SmoothUnion)",
    "Yumusak Cikar (SmoothSub)",
    "Yumusak Kesisim (SmoothIntersect)"
};

void Inspector::Draw(Scene& scene, Entity& selectedEntity) {
    ImGui::Begin("Bilesen Denetcisi");

    if (!selectedEntity.IsValid()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
        ImGui::Text("BILESEN DENETCISI");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8.0f));
        ImGui::TextDisabled("Duzenlemek icin bir nesne secin.");
        ImGui::End();
        return;
    }

    // ── Header ───────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
    ImGui::Text("BILESEN DENETCISI");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f));

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Secili Varlik: %s", selectedEntity.ToDisplayString().c_str());
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── 1. Transform Component ───────────────────────────────
    if (selectedEntity.HasComponent<TransformComponent>()) {
        if (ImGui::CollapsingHeader("Transform Bileseni", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            auto& transform = selectedEntity.GetComponent<TransformComponent>();

            ImGui::PushItemWidth(-1);

            ImGui::Text("Konum (m)");
            ImGui::DragFloat3("##Position", glm::value_ptr(transform.position), 0.05f);

            if (selectedEntity.HasComponent<HierarchyComponent>()) {
                const auto& hierarchy = selectedEntity.GetComponent<HierarchyComponent>();
                if (scene.GetRegistry().IsAlive(hierarchy.parent)) {
                    glm::vec3 worldPosition;
                    glm::quat worldRotation;
                    glm::vec3 worldScale;
                    DecomposeTransformMatrix(
                        scene.GetWorldTransform(selectedEntity.GetHandle()),
                        worldPosition,
                        worldRotation,
                        worldScale
                    );
                    ImGui::TextDisabled("World: %.2f, %.2f, %.2f  ·  Parent #%u",
                                        worldPosition.x, worldPosition.y, worldPosition.z,
                                        GetEntityIndex(hierarchy.parent));
                } else if (!hierarchy.children.empty()) {
                    ImGui::TextDisabled("Root  ·  %zu alt nesne", hierarchy.children.size());
                }
            }

            ImGui::Dummy(ImVec2(0, 2.0f));

            ImGui::Text("Rotasyon (Derece)");
            glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(transform.rotation));
            if (ImGui::DragFloat3("##Rotation", glm::value_ptr(eulerAngles), 1.0f)) {
                transform.rotation = glm::quat(glm::radians(eulerAngles));
            }

            // ── 3D Standart Kure Rotasyon Gizmosu (Interactive Trackball Sphere) ──
            ImGui::Dummy(ImVec2(0, 3.0f));
            ImGui::TextDisabled("Hizli rotasyon · surukle");
            
            float sphereWidgetRadius = 36.0f;
            float sphereWidgetHeight = sphereWidgetRadius * 2.0f + 6.0f;
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            float availWidth = ImGui::GetContentRegionAvail().x;
            ImVec2 sphereCenterPos = ImVec2(canvasPos.x + availWidth * 0.5f, canvasPos.y + sphereWidgetRadius + 3.0f);
            
            ImGui::InvisibleButton("##TrackballSphereGizmo", ImVec2(availWidth, sphereWidgetHeight));
            bool isSphereHovered = ImGui::IsItemHovered();
            bool isSphereActive = ImGui::IsItemActive();
            
            ImDrawList* iDrawList = ImGui::GetWindowDrawList();
            
            // Fare ile 3D Kureyi cevirme (Virtual Trackball / Arcball)
            if (isSphereActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                if (std::abs(delta.x) > 0.001f || std::abs(delta.y) > 0.001f) {
                    float speed = 0.015f;
                    glm::quat rotY = glm::angleAxis(delta.x * speed, glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::quat rotX = glm::angleAxis(delta.y * speed, glm::vec3(1.0f, 0.0f, 0.0f));
                    transform.rotation = glm::normalize(rotY * rotX * transform.rotation);
                }
            }
            
            // Kure Tabani ve 3D Golgeleme
            iDrawList->AddCircleFilled(sphereCenterPos, sphereWidgetRadius, IM_COL32(24, 28, 36, 230), 36);
            iDrawList->AddCircle(sphereCenterPos, sphereWidgetRadius, isSphereActive ? IM_COL32(65, 155, 255, 240) : isSphereHovered ? IM_COL32(95, 125, 160, 200) : IM_COL32(60, 70, 85, 150), 36, 1.6f);
            
            // 3D Kure Isik Vurgusu (Specular highlight)
            iDrawList->AddCircleFilled(ImVec2(sphereCenterPos.x - sphereWidgetRadius * 0.32f, sphereCenterPos.y - sphereWidgetRadius * 0.32f), sphereWidgetRadius * 0.5f, IM_COL32(255, 255, 255, 22), 24);
            
            // Nesnenin anlik rotasyon eksenlerini kure uzerinde 3D ciz (X Kirmizi, Y Yesil, Z Mavi)
            glm::mat3 objRot = glm::mat3_cast(transform.rotation);
            struct GizmoAxis {
                glm::vec3 dir;
                ImU32 color;
                const char* label;
            };
            GizmoAxis gAxes[3] = {
                { objRot[0], IM_COL32(235, 60, 65, 255), "X" },
                { objRot[1], IM_COL32(65, 215, 80, 255), "Y" },
                { objRot[2], IM_COL32(55, 145, 250, 255), "Z" }
            };
            
            for (const auto& gax : gAxes) {
                float armLen = sphereWidgetRadius * 0.72f;
                ImVec2 axTip(sphereCenterPos.x + gax.dir.x * armLen, sphereCenterPos.y - gax.dir.y * armLen);
                iDrawList->AddLine(sphereCenterPos, axTip, gax.color, 2.2f);
                iDrawList->AddCircleFilled(axTip, 4.0f, gax.color, 12);
                iDrawList->AddCircle(axTip, 4.0f, IM_COL32(255, 255, 255, 180), 12, 1.0f);
            }
            // Merkez pivot noktasi
            iDrawList->AddCircleFilled(sphereCenterPos, 3.0f, IM_COL32(200, 210, 225, 220), 16);

            ImGui::Dummy(ImVec2(0, 2.0f));

            ImGui::Text("Olcek");
            ImGui::DragFloat3("##Scale", glm::value_ptr(transform.scale), 0.02f, 0.01f, 10.0f);

            ImGui::PopItemWidth();
            ImGui::Unindent(8.0f);
        }
    }

    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── 2. SDF Component ─────────────────────────────────────
    if (selectedEntity.HasComponent<SDFComponent>()) {
        if (ImGui::CollapsingHeader("SDF Geometri & Materyal", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            auto& sdf = selectedEntity.GetComponent<SDFComponent>();

            ImGui::PushItemWidth(-1);

            // Primitive Type
            ImGui::Text("Sekil (Primitive)");
            int primType = static_cast<int>(sdf.primitiveType);
            if (ImGui::Combo("##Primitive", &primType, s_PrimitiveNames, IM_ARRAYSIZE(s_PrimitiveNames))) {
                sdf.primitiveType = static_cast<uint32_t>(primType);
            }

            ImGui::Dummy(ImVec2(0, 2.0f));

            // CSG Operation
            ImGui::Text("CSG Islemi");
            int opType = static_cast<int>(sdf.operation);
            if (ImGui::Combo("##Operation", &opType, s_OperationNames, IM_ARRAYSIZE(s_OperationNames))) {
                sdf.operation = static_cast<uint32_t>(opType);
            }

            ImGui::Dummy(ImVec2(0, 2.0f));

            // Blend factor
            ImGui::Text("Yumusaklik (Blend k)");
            ImGui::SliderFloat("##Blend", &sdf.blendFactor, 0.0f, 1.5f, "%.2f m");

            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 6.0f));

            // ── Material sub-section ─────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
            ImGui::Text("MATERYAL");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4.0f));

            ImGui::PushItemWidth(-1);

            ImGui::Text("Albedo Rengi");
            ImGui::ColorEdit3("##Albedo", glm::value_ptr(sdf.albedo));

            ImGui::Dummy(ImVec2(0, 2.0f));

            ImGui::Text("Puruzluluk (Roughness)");
            ImGui::SliderFloat("##Roughness", &sdf.roughness, 0.0f, 1.0f);

            ImGui::Dummy(ImVec2(0, 2.0f));

            ImGui::Text("Metalik (Metallic)");
            ImGui::SliderFloat("##Metallic", &sdf.metallic, 0.0f, 1.0f);

            ImGui::PopItemWidth();
            ImGui::Unindent(8.0f);
        }
    }

    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── 3. Velocity Component ────────────────────────────────
    if (selectedEntity.HasComponent<VelocityComponent>()) {
        if (ImGui::CollapsingHeader("Fizik & Kinematik Hiz", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            auto& vel = selectedEntity.GetComponent<VelocityComponent>();

            ImGui::PushItemWidth(-1);

            ImGui::Text("Cizgisel Hiz (m/s)");
            ImGui::DragFloat3("##LinearVelocity", glm::value_ptr(vel.linear), 0.1f);

            ImGui::Dummy(ImVec2(0, 2.0f));

            ImGui::Text("Acisal Hiz (rad/s)");
            ImGui::DragFloat3("##AngularVelocity", glm::value_ptr(vel.angular), 0.1f);

            ImGui::PopItemWidth();
            ImGui::Unindent(8.0f);
        }
    }

    ImGui::End();
}

} // namespace Astral
