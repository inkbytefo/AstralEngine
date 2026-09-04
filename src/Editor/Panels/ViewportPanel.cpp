#include "Astral/Editor/Panels/ViewportPanel.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/InputSystem.hpp"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace Astral {

ViewportPanel::ViewportPanel(SDFRenderer* renderer, const InputSystem* input)
    : m_Renderer(renderer), m_Input(input) {}

void ViewportPanel::OnImGuiRender() {
    Entity nullEntity;
    Scene dummyScene("Dummy");
    OnImGuiRender(dummyScene, nullEntity);
}

void ViewportPanel::OnImGuiRender(Scene& scene, Entity& selectedEntity) {
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

    const glm::dvec2 rawMousePos = m_Input ? m_Input->GetMousePosition() : glm::dvec2(0.0);
    ImVec2 mousePos(static_cast<float>(rawMousePos.x), static_cast<float>(rawMousePos.y));
    glm::vec2 mousePosInViewport = { mousePos.x - viewportBoundsMin.x, mousePos.y - viewportBoundsMin.y };

    // Clamp coordinates within viewport limits
    mousePosInViewport.x = std::clamp(mousePosInViewport.x, 0.0f, std::max(0.0f, m_ViewportSize.x - 1.0f));
    mousePosInViewport.y = std::clamp(mousePosInViewport.y, 0.0f, std::max(0.0f, m_ViewportSize.y - 1.0f));
    m_MousePosInViewport = mousePosInViewport;

    if (m_Input) {
        m_TransformGizmo.UpdateShortcuts(*m_Input, m_IsFocused || m_IsHovered, ImGui::GetIO().WantTextInput);
    }

    // Camera matrices matching SDFCompute.glsl
    glm::vec3 camPos = glm::vec3(0.0f, 1.5f, 4.0f);
    glm::vec3 camDir = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
    glm::mat4 view = glm::lookAt(camPos, camPos + camDir, glm::vec3(0.0f, 1.0f, 0.0f));

    float fovY = 2.0f * std::atan(0.5f / 1.5f); // ~36.87 deg
    float aspect = (m_ViewportSize.y > 0.0f) ? (m_ViewportSize.x / m_ViewportSize.y) : (16.0f / 9.0f);
    glm::mat4 proj = glm::perspective(fovY, aspect, 0.1f, 100.0f);

    m_TransformGizmo.Manipulate(scene, selectedEntity, view, proj,
        {viewportBoundsMin.x, viewportBoundsMin.y, m_ViewportSize.x, m_ViewportSize.y}, m_Input);

    // 6. Send picking request to GPU compute if user clicked inside hovered viewport (and not interacting with Gizmo)
    const bool isGizmoInteracting = m_TransformGizmo.State().IsInteracting();
    if (m_Input && m_Input->IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && m_IsHovered && !isGizmoInteracting) {
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
    const auto& gizmoState = m_TransformGizmo.State();
    const bool snapping = m_Input && (m_Input->IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                                      m_Input->IsKeyPressed(GLFW_KEY_RIGHT_CONTROL));
    std::snprintf(infoText, sizeof(infoText), "SDF  %.0f x %.0f  |  %s · %s%s",
                  m_ViewportSize.x, m_ViewportSize.y, GizmoOperationName(gizmoState.operation),
                  GizmoSpaceName(gizmoState.space), snapping ? " · Snap" : "");
    
    ImVec2 badgeTextSize = ImGui::CalcTextSize(infoText);
    ImVec2 badgeRectMin = ImVec2(badgePos.x - 6.0f, badgePos.y - 4.0f);
    ImVec2 badgeRectMax = ImVec2(badgePos.x + badgeTextSize.x + 16.0f, badgePos.y + badgeTextSize.y + 4.0f);
    drawList->AddRectFilled(badgeRectMin, badgeRectMax, IM_COL32(18, 20, 26, 190), 6.0f);
    drawList->AddRect(badgeRectMin, badgeRectMax, IM_COL32(60, 70, 85, 120), 6.0f, 0, 1.0f);
    drawList->AddCircleFilled(ImVec2(badgePos.x, badgePos.y + badgeTextSize.y * 0.5f), 3.0f, IM_COL32(45, 145, 245, 255));
    drawList->AddText(ImVec2(badgePos.x + 8.0f, badgePos.y), IM_COL32(230, 235, 245, 240), infoText);

    m_GizmoToolbar.Draw(m_TransformGizmo.State(),
        {viewportBoundsMin.x, viewportBoundsMin.y}, m_ViewportSize, view);

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Astral
