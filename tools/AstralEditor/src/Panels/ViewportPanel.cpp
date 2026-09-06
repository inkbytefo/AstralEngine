#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Editor/Panels/ViewportPanel.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Core/InputSystem.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace Astral {

ViewportPanel::ViewportPanel(SDFRenderer* renderer, const InputSystem* input)
    : m_Renderer(renderer), m_Input(input) {}

ViewportPanel::~ViewportPanel() {
    CleanupDescriptorSet();
}

void ViewportPanel::CleanupDescriptorSet() {
    if (m_ViewportDescriptorSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_ViewportDescriptorSet);
        m_ViewportDescriptorSet = VK_NULL_HANDLE;
    }
    m_RegisteredImageView = VK_NULL_HANDLE;
}

void ViewportPanel::OnImGuiRender(bool isPlaying) {
    SelectionContext selection;
    Scene dummyScene("Dummy");
    OnImGuiRender(dummyScene, selection, isPlaying);
}

void ViewportPanel::OnImGuiRender(Scene& scene, SelectionContext& selection, bool isPlaying) {
    m_TransportAction = ViewportTransportAction::None;
    // Clear transient interaction even when the viewport is hidden or paused.
    // Preserve the user's operation and coordinate space for returning to Edit.
    m_TransformGizmo.State().usingGizmo = false;
    m_TransformGizmo.State().hoveringGizmo = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    if (!ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        m_IsHovered = false;
        m_IsFocused = false;
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    m_IsHovered = ImGui::IsWindowHovered();
    m_IsFocused = ImGui::IsWindowFocused();

    {
        if (!isPlaying && m_Input) m_TransformGizmo.UpdateShortcuts(*m_Input, m_IsFocused || m_IsHovered, ImGui::GetIO().WantTextInput);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const auto toolbarCamera = ExtractActiveCamera(scene.GetRegistry(), available.y > 44 ? available.x / (available.y - 44) : 1.0f);
        m_TransportAction = m_GizmoToolbar.Draw(m_TransformGizmo.State(), {origin.x, origin.y}, {available.x, available.y},
                            toolbarCamera ? toolbarCamera->view : glm::mat4(1), isPlaying);
    }
    // Picking, render sizing and ImGuizmo all use the image's actual origin.
    const ImVec2 viewportBoundsMin = ImGui::GetCursorScreenPos();
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    int targetW = static_cast<int>(viewportPanelSize.x);
    int targetH = static_cast<int>(viewportPanelSize.y);

    // 1. Dynamic Resize & Aspect Ratio (Between-frame deferred resize for Vulkan safety)
    if (m_Renderer && targetW > 0 && targetH > 0) {
        if (targetW != m_Renderer->GetWidth() || targetH != m_Renderer->GetHeight()) {
            m_PendingResize = true;
            m_PendingSize = { static_cast<float>(targetW), static_cast<float>(targetH) };
        }
    }

    m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

    // 2. Render Vulkan Image in ImGui
    VkImageView currentImageView = m_Renderer ? static_cast<VkImageView>(m_Renderer->GetStorageImageView()) : VK_NULL_HANDLE;

    // Eger render hedefi boyutu degismis veya goruntu yeniden olusturulmussa eski descriptor set'i temizle
    if (m_RegisteredImageView != currentImageView) {
        CleanupDescriptorSet();
    }

    if (currentImageView != VK_NULL_HANDLE && m_Renderer->GetViewportSampler() && m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f) {
        if (m_ViewportDescriptorSet == VK_NULL_HANDLE) {
            m_ViewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
                static_cast<VkSampler>(m_Renderer->GetViewportSampler()),
                currentImageView,
                VK_IMAGE_LAYOUT_GENERAL
            );
            m_RegisteredImageView = currentImageView;
        }
        ImTextureID textureID = reinterpret_cast<ImTextureID>(m_ViewportDescriptorSet);
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
    const glm::dvec2 rawMousePos = m_Input ? m_Input->GetMousePosition() : glm::dvec2(0.0);
    ImVec2 mousePos(static_cast<float>(rawMousePos.x), static_cast<float>(rawMousePos.y));
    glm::vec2 mousePosInViewport = { mousePos.x - viewportBoundsMin.x, mousePos.y - viewportBoundsMin.y };

    // Clamp coordinates within viewport limits
    mousePosInViewport.x = std::clamp(mousePosInViewport.x, 0.0f, std::max(0.0f, m_ViewportSize.x - 1.0f));
    mousePosInViewport.y = std::clamp(mousePosInViewport.y, 0.0f, std::max(0.0f, m_ViewportSize.y - 1.0f));
    m_MousePosInViewport = mousePosInViewport;

    const ImVec2 uiMouse = ImGui::GetMousePos();
    const bool overContext = !isPlaying && m_TransformGizmo.State().IsEnabled() && m_ViewportSize.x >= 160 && m_ViewportSize.y >= 220 &&
        uiMouse.x >= viewportBoundsMin.x + 8 && uiMouse.x <= viewportBoundsMin.x + 74 &&
        uiMouse.y >= viewportBoundsMin.y + 8 && uiMouse.y <= viewportBoundsMin.y + 198;
    m_IsHovered = m_IsHovered && !overContext && uiMouse.x >= viewportBoundsMin.x &&
        uiMouse.y >= viewportBoundsMin.y && uiMouse.x < viewportBoundsMin.x + m_ViewportSize.x &&
        uiMouse.y < viewportBoundsMin.y + m_ViewportSize.y;

    const float aspect = m_ViewportSize.y > 0 ? m_ViewportSize.x / m_ViewportSize.y : 1.0f;
    const auto camera = ExtractActiveCamera(scene.GetRegistry(), aspect);
    if (!isPlaying && camera && !overContext) {
        m_TransformGizmo.Manipulate(scene, selection, camera->view, camera->projection,
            {viewportBoundsMin.x, viewportBoundsMin.y, m_ViewportSize.x, m_ViewportSize.y}, m_Input);
    }
    // 6. Send picking request to GPU compute if user clicked inside hovered viewport (and not interacting with Gizmo)
    const bool isGizmoInteracting = m_TransformGizmo.State().IsInteracting();
    if (!m_PickPending && !isPlaying && m_Input && m_Input->IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && m_IsHovered && !isGizmoInteracting) {
        if (m_Renderer && m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f) {
            m_PickPending = true;
            m_PickAdditive = ImGui::GetIO().KeyCtrl;
            m_Renderer->SetPickingRequest(
                static_cast<int>(mousePosInViewport.x),
                static_cast<int>(mousePosInViewport.y)
            );
        }
    }

    // 7. Viewport Overlay Elements
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Bottom-left: compact opaque viewport information, clear of the gizmo toolbar.
    ImVec2 badgePos = ImVec2(viewportBoundsMin.x + 12.0f, viewportBoundsMin.y + m_ViewportSize.y - ImGui::GetTextLineHeight() - 12.0f);
    if (isPlaying) badgePos.y = viewportBoundsMin.y + 12.0f;
    char infoText[128];
    const auto& gizmoState = m_TransformGizmo.State();
    const bool snapping = m_Input && (m_Input->IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                                      m_Input->IsKeyPressed(GLFW_KEY_RIGHT_CONTROL));
    if (isPlaying) {
        std::snprintf(infoText, sizeof(infoText), "GAME VIEW (Runtime)");
    } else {
        std::snprintf(infoText, sizeof(infoText), "SDF  %.0f x %.0f  |  %s · %s%s",
                  m_ViewportSize.x, m_ViewportSize.y, GizmoOperationName(gizmoState.operation),
                  GizmoSpaceName(gizmoState.space), snapping ? " · Snap" : "");
    }
    
    ImVec2 badgeTextSize = ImGui::CalcTextSize(infoText);
    ImVec2 badgeRectMin = ImVec2(badgePos.x - 6.0f, badgePos.y - 4.0f);
    ImVec2 badgeRectMax = ImVec2(badgePos.x + badgeTextSize.x + 16.0f, badgePos.y + badgeTextSize.y + 4.0f);
    drawList->AddRectFilled(badgeRectMin, badgeRectMax, IM_COL32(23, 25, 28, 255), 4.0f);
    drawList->AddRect(badgeRectMin, badgeRectMax, IM_COL32(60, 65, 72, 255), 4.0f, 0, 1.0f);
    drawList->AddCircleFilled(ImVec2(badgePos.x, badgePos.y + badgeTextSize.y * 0.5f), 3.0f,
                             isPlaying ? IM_COL32(118, 186, 133, 255) : IM_COL32(56, 107, 191, 255));
    drawList->AddText(ImVec2(badgePos.x + 8.0f, badgePos.y), IM_COL32(230, 235, 245, 240), infoText);

    if (!isPlaying) {
        m_GizmoToolbar.DrawContext(m_TransformGizmo.State(),
            {viewportBoundsMin.x, viewportBoundsMin.y}, m_ViewportSize);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Astral


