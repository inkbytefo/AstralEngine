#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <cstdint>

struct ImDrawList;

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
    ~ViewportPanel() = default;

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

    [[nodiscard]] int GetGizmoType() const noexcept { return m_GizmoType; }
    void SetGizmoType(int type) noexcept { m_GizmoType = type; }

    [[nodiscard]] int GetGizmoMode() const noexcept { return m_GizmoMode; }
    void SetGizmoMode(int mode) noexcept { m_GizmoMode = mode; }

    [[nodiscard]] bool IsUsingGizmo() const noexcept { return m_IsUsingGizmo; }

private:
    SDFRenderer* m_Renderer = nullptr;
    const InputSystem* m_Input = nullptr;
    glm::vec2 m_ViewportSize{ 0.0f, 0.0f };
    glm::vec2 m_PendingSize{ 0.0f, 0.0f };
    glm::vec2 m_MousePosInViewport{ -1.0f, -1.0f };
    bool m_PendingResize = false;
    bool m_IsHovered = false;
    bool m_IsFocused = false;
    bool m_IsUsingGizmo = false;
    bool m_GizmoStyleInitialized = false;

    // Gizmo state: 0 = Translate, 1 = Rotate, 2 = Scale, 3 = Universal, -1 = None
    int m_GizmoType = 0;
    // Gizmo coordinate space: 0 = World, 1 = Local
    int m_GizmoMode = 0;

    /// Renders Blender-standard 3D Navigation Sphere Gizmo in the top-right corner of the Viewport
    void DrawNavigationSphere(::ImDrawList* drawList, const glm::vec2& center, float radius, const glm::mat4& view);
};

} // namespace Astral
