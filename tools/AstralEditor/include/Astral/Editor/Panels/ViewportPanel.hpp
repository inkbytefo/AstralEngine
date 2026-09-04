#pragma once

#include "Astral/Editor/Gizmo/TransformGizmo.hpp"
#include "Astral/Editor/Gizmo/ViewportGizmoToolbar.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

#include <vulkan/vulkan.h>

namespace Astral {

class SDFRenderer;
class InputSystem;
class Scene;
class Entity;

/**
 * @brief ViewportPanel embeds the Vulkan SDF Compute raymarched output
 *        into a dockable ImGui editor window.
 *
 * Handles dynamic viewport resizing, aspect ratio adaptation, relative
 * mouse coordinate transformations for high-precision entity picking,
 * and 3D Transform Gizmo manipulation (ImGuizmo).
 */
class ViewportPanel {
public:
    ViewportPanel() = default;
    explicit ViewportPanel(SDFRenderer* renderer, const InputSystem* input = nullptr);
    ~ViewportPanel();

    void CleanupDescriptorSet();

    void SetRenderer(SDFRenderer* renderer) noexcept { m_Renderer = renderer; }
    void SetInputSystem(const InputSystem* input) noexcept { m_Input = input; }
    [[nodiscard]] SDFRenderer* GetRenderer() const noexcept { return m_Renderer; }

    /// Primary interface called during ImGui editor rendering with ECS Scene & Entity
    void OnImGuiRender(Scene& scene, Entity& selectedEntity);

    /// Overloads for standalone or legacy calls
    void OnImGuiRender();
    void Draw(Scene& scene, Entity& selectedEntity) { OnImGuiRender(scene, selectedEntity); }
    void Draw() { OnImGuiRender(); }

    [[nodiscard]] glm::vec2 GetViewportSize() const noexcept { return m_ViewportSize; }
    [[nodiscard]] glm::vec2 GetMousePosInViewport() const noexcept { return m_MousePosInViewport; }
    [[nodiscard]] bool IsHovered() const noexcept { return m_IsHovered; }
    [[nodiscard]] bool IsFocused() const noexcept { return m_IsFocused; }

    [[nodiscard]] bool HasPendingResize() const noexcept { return m_PendingResize; }
    [[nodiscard]] glm::vec2 GetPendingResize() const noexcept { return m_PendingSize; }
    void ClearPendingResize() noexcept { m_PendingResize = false; }

    [[nodiscard]] int GetGizmoType() const noexcept { return static_cast<int>(m_TransformGizmo.State().operation); }
    void SetGizmoType(int type) noexcept { m_TransformGizmo.State().operation = static_cast<GizmoOperation>(type); }

    [[nodiscard]] int GetGizmoMode() const noexcept { return static_cast<int>(m_TransformGizmo.State().space); }
    void SetGizmoMode(int mode) noexcept { m_TransformGizmo.State().space = static_cast<GizmoSpace>(mode); }

    [[nodiscard]] bool IsUsingGizmo() const noexcept { return m_TransformGizmo.State().usingGizmo; }

private:
    SDFRenderer* m_Renderer = nullptr;
    const InputSystem* m_Input = nullptr;
    glm::vec2 m_ViewportSize{ 0.0f, 0.0f };
    glm::vec2 m_PendingSize{ 0.0f, 0.0f };
    glm::vec2 m_MousePosInViewport{ -1.0f, -1.0f };
    bool m_PendingResize = false;
    bool m_IsHovered = false;
    bool m_IsFocused = false;
    TransformGizmo m_TransformGizmo;
    ViewportGizmoToolbar m_GizmoToolbar;
    VkDescriptorSet m_ViewportDescriptorSet = VK_NULL_HANDLE;
    VkImageView m_RegisteredImageView = VK_NULL_HANDLE;
};

} // namespace Astral
