#include "Astral/Editor/Panels/ViewportPanel.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/Components.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace Astral {

ViewportPanel::ViewportPanel(SDFRenderer* renderer)
    : m_Renderer(renderer) {}

void ViewportPanel::OnImGuiRender() {
    Entity nullEntity;
    Scene dummyScene("Dummy");
    OnImGuiRender(dummyScene, nullEntity);
}

void ViewportPanel::OnImGuiRender(Scene& scene, Entity& selectedEntity) {
    (void)scene;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_IsHovered = ImGui::IsWindowHovered();
    m_IsFocused = ImGui::IsWindowFocused();

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    // 1. Dynamic Resize & Aspect Ratio (Between-frame deferred resize for Vulkan safety)
    if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f) {
        glm::vec2 newSize = { viewportPanelSize.x, viewportPanelSize.y };
        if (m_ViewportSize != newSize) {
            m_PendingResize = true;
            m_PendingSize = newSize;
            m_ViewportSize = newSize;
        }
    }

    // 2. Render Vulkan Image in ImGui
    if (m_Renderer && m_Renderer->GetViewportTextureID() && m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f) {
        ImTextureID textureID = reinterpret_cast<ImTextureID>(m_Renderer->GetViewportTextureID());
        ImGui::Image(textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y });
    } else {
        // Fallback placeholder while initializing
        ImVec2 center = ImGui::GetCursorScreenPos();
        center.x += m_ViewportSize.x * 0.5f;
        center.y += m_ViewportSize.y * 0.5f;
        const char* msg = "SDF Compute Viewport Yukleniyor...";
        ImVec2 tSize = ImGui::CalcTextSize(msg);
        ImGui::SetCursorScreenPos(ImVec2(center.x - tSize.x * 0.5f, center.y - tSize.y * 0.5f));
        ImGui::TextUnformatted(msg);
    }

    // 3. Mouse Coordinate Transformation (Relative Picking)
    ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
    ImVec2 viewportOffset = ImGui::GetWindowPos();
    ImVec2 viewportBoundsMin = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };

    ImVec2 mousePos = ImGui::GetMousePos();
    glm::vec2 mousePosInViewport = { mousePos.x - viewportBoundsMin.x, mousePos.y - viewportBoundsMin.y };

    // Clamp coordinates within viewport limits
    mousePosInViewport.x = std::clamp(mousePosInViewport.x, 0.0f, std::max(0.0f, m_ViewportSize.x - 1.0f));
    mousePosInViewport.y = std::clamp(mousePosInViewport.y, 0.0f, std::max(0.0f, m_ViewportSize.y - 1.0f));
    m_MousePosInViewport = mousePosInViewport;

    // 4. Shortcut Keys for Gizmo Operation (W: Translate, E: Rotate, R: Scale, Q: None)
    if (m_IsFocused || m_IsHovered) {
        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { m_GizmoType = 0; } // Translate
            if (ImGui::IsKeyPressed(ImGuiKey_E)) { m_GizmoType = 1; } // Rotate (3D Sphere)
            if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_GizmoType = 2; } // Scale
            if (ImGui::IsKeyPressed(ImGuiKey_T)) { m_GizmoType = 3; } // Universal 3D Sphere Gizmo
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) { m_GizmoType = -1; } // None / Select
        }
    }

    // Camera matrices matching SDFCompute.glsl
    glm::vec3 camPos = glm::vec3(0.0f, 1.5f, 4.0f);
    glm::vec3 camDir = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
    glm::mat4 view = glm::lookAt(camPos, camPos + camDir, glm::vec3(0.0f, 1.0f, 0.0f));

    float fovY = 2.0f * std::atan(0.5f / 1.5f); // ~36.87 deg
    float aspect = (m_ViewportSize.y > 0.0f) ? (m_ViewportSize.x / m_ViewportSize.y) : (16.0f / 9.0f);
    glm::mat4 proj = glm::perspective(fovY, aspect, 0.1f, 100.0f);

    // 5. ImGuizmo 3D Transform Manipulation (3D Standart Kure & Eksenler)
    m_IsUsingGizmo = false;
    if (selectedEntity.IsValid() && selectedEntity.HasComponent<TransformComponent>() && m_GizmoType >= 0) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportBoundsMin.x, viewportBoundsMin.y, m_ViewportSize.x, m_ViewportSize.y);

        // Modern 3D Kure Gizmo Stili (Kalinlastirilmis PBR antialiased cizgiler & merkez kure)
        auto& gizmoStyle = ImGuizmo::GetStyle();
        gizmoStyle.TranslationLineThickness = 3.5f;
        gizmoStyle.TranslationLineArrowSize = 8.0f;
        gizmoStyle.RotationLineThickness = 3.5f;
        gizmoStyle.RotationOuterLineThickness = 3.5f;
        gizmoStyle.ScaleLineThickness = 3.5f;
        gizmoStyle.ScaleLineCircleSize = 7.0f;
        gizmoStyle.CenterCircleSize = 6.5f;
        gizmoStyle.HatchedAxisLineThickness = 2.5f;

        gizmoStyle.Colors[ImGuizmo::DIRECTION_X] = ImVec4(0.95f, 0.22f, 0.22f, 1.0f);
        gizmoStyle.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.22f, 0.85f, 0.35f, 1.0f);
        gizmoStyle.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.22f, 0.55f, 0.95f, 1.0f);
        gizmoStyle.Colors[ImGuizmo::PLANE_X]     = ImVec4(0.95f, 0.22f, 0.22f, 0.45f);
        gizmoStyle.Colors[ImGuizmo::PLANE_Y]     = ImVec4(0.22f, 0.85f, 0.35f, 0.45f);
        gizmoStyle.Colors[ImGuizmo::PLANE_Z]     = ImVec4(0.22f, 0.55f, 0.95f, 0.45f);
        gizmoStyle.Colors[ImGuizmo::ROTATION_USING_FILL]   = ImVec4(0.20f, 0.55f, 0.95f, 0.35f);
        gizmoStyle.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.00f, 0.85f, 0.25f, 1.0f);

        auto& tc = selectedEntity.GetComponent<TransformComponent>();
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.position)
                            * glm::mat4_cast(tc.rotation)
                            * glm::scale(glm::mat4(1.0f), tc.scale);

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        if (m_GizmoType == 1) op = ImGuizmo::ROTATE;
        else if (m_GizmoType == 2) op = ImGuizmo::SCALE;
        else if (m_GizmoType == 3) op = ImGuizmo::UNIVERSAL; // 3D Kure + Oklu Evrensel Gizmo

        ImGuizmo::MODE mode = (m_GizmoMode == 1) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        // Snapping (Ctrl key modifier)
        bool snap = ImGui::GetIO().KeyCtrl;
        float snapValue = (m_GizmoType == 1) ? 45.0f : 0.5f; // 45 deg or 0.5m
        float snapValues[3] = { snapValue, snapValue, snapValue };

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            op,
            mode,
            glm::value_ptr(transform),
            nullptr,
            snap ? snapValues : nullptr
        );

        if (ImGuizmo::IsUsing()) {
            m_IsUsingGizmo = true;
            float matrixTranslation[3], matrixRotation[3], matrixScale[3];
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(transform),
                matrixTranslation,
                matrixRotation,
                matrixScale
            );

            tc.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
            tc.scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);

            // Quaternion conversion from Euler angles in degrees (prevents Gimbal Lock)
            glm::vec3 radEuler = glm::radians(glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]));
            tc.rotation = glm::quat(radEuler);
        }
    }

    // 6. Send picking request to GPU compute if user clicked inside hovered viewport (and not interacting with Gizmo)
    bool isGizmoInteracting = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_IsHovered && !isGizmoInteracting) {
        if (m_Renderer && m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f) {
            m_Renderer->SetPickingRequest(
                static_cast<int>(mousePosInViewport.x),
                static_cast<int>(mousePosInViewport.y)
            );
        }
    }

    // 7. Viewport Overlay Elements
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Top-Left: Modern Glassmorphic HUD Badge
    ImVec2 badgePos = ImVec2(viewportBoundsMin.x + 12.0f, viewportBoundsMin.y + 12.0f);
    char infoText[128];
    const char* opName = (m_GizmoType == 0) ? "Translate (W)" 
                       : (m_GizmoType == 1) ? "Rotate Sphere (E)" 
                       : (m_GizmoType == 2) ? "Scale (R)" 
                       : (m_GizmoType == 3) ? "Universal Sphere (T)" : "Select (Q)";
    std::snprintf(infoText, sizeof(infoText), "SDF Viewport: %.0fx%.0f  |  Gizmo: %s [%s]", 
                  m_ViewportSize.x, m_ViewportSize.y, opName, (m_GizmoMode == 0) ? "World" : "Local");
    
    ImVec2 badgeTextSize = ImGui::CalcTextSize(infoText);
    ImVec2 badgeRectMin = ImVec2(badgePos.x - 6.0f, badgePos.y - 4.0f);
    ImVec2 badgeRectMax = ImVec2(badgePos.x + badgeTextSize.x + 16.0f, badgePos.y + badgeTextSize.y + 4.0f);
    drawList->AddRectFilled(badgeRectMin, badgeRectMax, IM_COL32(18, 20, 26, 190), 6.0f);
    drawList->AddRect(badgeRectMin, badgeRectMax, IM_COL32(60, 70, 85, 120), 6.0f, 0, 1.0f);
    drawList->AddCircleFilled(ImVec2(badgePos.x, badgePos.y + badgeTextSize.y * 0.5f), 3.0f, IM_COL32(45, 145, 245, 255));
    drawList->AddText(ImVec2(badgePos.x + 8.0f, badgePos.y), IM_COL32(230, 235, 245, 240), infoText);

    // Top-Right: 3D Standart Navigasyon Kure Gizmosu (Blender 3D Navigation Sphere)
    float sphereRadius = 34.0f;
    glm::vec2 sphereCenter(
        viewportBoundsMin.x + m_ViewportSize.x - sphereRadius - 16.0f,
        viewportBoundsMin.y + sphereRadius + 14.0f
    );
    DrawNavigationSphere(drawList, sphereCenter, sphereRadius, view);

    // Modern Yüzer Transform Araç Çubuğu (Küre Gizmosunun Solunda)
    float toolbarWidth = 230.0f;
    float toolbarHeight = 30.0f;
    ImVec2 toolbarPos = ImVec2(
        sphereCenter.x - sphereRadius - toolbarWidth - 14.0f,
        viewportBoundsMin.y + 12.0f
    );

    ImGui::SetCursorScreenPos(toolbarPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.15f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 3.0f));

    if (ImGui::BeginChild("ViewportGizmoToolbar", ImVec2(toolbarWidth, toolbarHeight), false, ImGuiWindowFlags_NoScrollbar)) {
        auto drawToolButton = [this](const char* label, int type, const char* tooltip) {
            bool active = (m_GizmoType == type);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.46f, 0.88f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.22f, 0.6f));
            }
            if (ImGui::Button(label, ImVec2(30, 24))) {
                m_GizmoType = type;
            }
            if (ImGui::IsItemHovered() && tooltip) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
        };

        drawToolButton("W", 0, "Tasi (Translate - W)");
        drawToolButton("E", 1, "Dondur Kure (Rotate Sphere - E)");
        drawToolButton("R", 2, "Olcekle (Scale - R)");
        drawToolButton("T", 3, "3D Evrensel Kure (Universal Gizmo - T)");

        const char* modeLabel = (m_GizmoMode == 0) ? "World" : "Local";
        if (ImGui::Button(modeLabel, ImVec2(52, 24))) {
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Koordinat Sistemi: %s", (m_GizmoMode == 0) ? "Dunya (World)" : "Yerel (Local)");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::DrawNavigationSphere(ImDrawList* drawList, const glm::vec2& center, float radius, const glm::mat4& view) {
    if (!drawList) return;

    ImVec2 centerPos(center.x, center.y);

    // 1. Shaded Sphere Body (Derinlikli Kure Tabanı)
    drawList->AddCircleFilled(centerPos, radius, IM_COL32(20, 24, 30, 215), 36);
    drawList->AddCircle(centerPos, radius, IM_COL32(70, 80, 95, 160), 36, 1.5f);

    // 3D Kure Parlaması (Specular highlight)
    ImVec2 highlightPos(centerPos.x - radius * 0.28f, centerPos.y - radius * 0.28f);
    drawList->AddCircleFilled(highlightPos, radius * 0.52f, IM_COL32(255, 255, 255, 18), 24);

    // 2. 3D Koordinat Eksenlerinin Kamera Donusune Gore Hesaplanmasi
    glm::mat3 camRot = glm::mat3(view);

    struct AxisInfo {
        const char* label;
        glm::vec3 dir;
        ImU32 posColor;
        ImU32 negColor;
        float depth;
        glm::vec2 screenPos;
        bool isPositive;
    };

    std::vector<AxisInfo> axes = {
        { "X", glm::vec3( 1.0f,  0.0f,  0.0f), IM_COL32(235, 55, 65, 255),  IM_COL32(130, 40, 45, 140), 0.0f, {}, true },
        { "-X", glm::vec3(-1.0f,  0.0f,  0.0f), IM_COL32(235, 55, 65, 255),  IM_COL32(100, 35, 40, 120), 0.0f, {}, false },
        { "Y", glm::vec3( 0.0f,  1.0f,  0.0f), IM_COL32(65, 215, 80, 255),  IM_COL32(40, 120, 50, 140), 0.0f, {}, true },
        { "-Y", glm::vec3( 0.0f, -1.0f,  0.0f), IM_COL32(65, 215, 80, 255),  IM_COL32(35, 95, 45, 120), 0.0f, {}, false },
        { "Z", glm::vec3( 0.0f,  0.0f,  1.0f), IM_COL32(50, 145, 250, 255), IM_COL32(35, 85, 150, 140), 0.0f, {}, true },
        { "-Z", glm::vec3( 0.0f,  0.0f, -1.0f), IM_COL32(50, 145, 250, 255), IM_COL32(30, 70, 125, 120), 0.0f, {}, false }
    };

    float armLength = radius * 0.72f;

    for (auto& ax : axes) {
        glm::vec3 proj = camRot * ax.dir;
        ax.depth = proj.z;
        ax.screenPos = glm::vec2(center.x + proj.x * armLength, center.y - proj.y * armLength);
    }

    // Derinlige gore arkadan one dogru siralama (Depth Sorting)
    std::sort(axes.begin(), axes.end(), [](const AxisInfo& a, const AxisInfo& b) {
        return a.depth < b.depth;
    });

    ImVec2 mousePos = ImGui::GetIO().MousePos;

    // 3. Arka Eksenleri Ciz (Kameraya uzak olan negatif eksenler)
    for (const auto& ax : axes) {
        if (ax.depth <= 0.0f) {
            ImVec2 p(ax.screenPos.x, ax.screenPos.y);
            drawList->AddLine(centerPos, p, IM_COL32(65, 70, 80, 120), 1.0f);
            drawList->AddCircleFilled(p, 3.0f, ax.negColor, 10);
        }
    }

    // 4. Merkez Kure Pivot Noktasi
    drawList->AddCircleFilled(centerPos, 3.0f, IM_COL32(180, 195, 210, 200), 16);

    // 5. On Eksenleri ve Etkilesimli 3D Kure Dugmelerini Ciz
    for (const auto& ax : axes) {
        if (ax.depth > 0.0f) {
            ImVec2 p(ax.screenPos.x, ax.screenPos.y);
            drawList->AddLine(centerPos, p, ax.posColor, 2.2f);

            if (ax.isPositive) {
                float nodeR = 8.0f;
                float dMouse = std::hypot(mousePos.x - p.x, mousePos.y - p.y);
                bool isHovered = (dMouse <= nodeR + 2.0f);
                if (isHovered) {
                    nodeR = 9.5f;
                }

                // Kure basligi golgesi & govdesi
                drawList->AddCircleFilled(ImVec2(p.x + 0.8f, p.y + 1.2f), nodeR, IM_COL32(0, 0, 0, 130), 16);
                drawList->AddCircleFilled(p, nodeR, isHovered ? IM_COL32(255, 255, 255, 255) : ax.posColor, 16);
                drawList->AddCircle(p, nodeR, IM_COL32(255, 255, 255, 180), 16, 1.0f);

                // Eksen harfi (X, Y, Z)
                ImVec2 textSize = ImGui::CalcTextSize(ax.label);
                ImVec2 textPos(p.x - textSize.x * 0.5f, p.y - textSize.y * 0.5f);
                drawList->AddText(textPos, isHovered ? ax.posColor : IM_COL32(255, 255, 255, 255), ax.label);
            } else {
                drawList->AddCircleFilled(p, 3.0f, ax.negColor, 10);
            }
        }
    }
}

} // namespace Astral

