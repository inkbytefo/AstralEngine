#pragma once

#include "Astral/Editor/Gizmo/GizmoState.hpp"
#include <glm/glm.hpp>

namespace Astral {

class Entity;
class InputSystem;
class Scene;

struct GizmoViewportRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

class TransformGizmo {
public:
    void UpdateShortcuts(const InputSystem& input, bool viewportActive, bool textInputActive);
    bool Manipulate(Scene& scene, Entity& entity, const glm::mat4& view,
                    const glm::mat4& projection, const GizmoViewportRect& viewport,
                    const InputSystem* input);

    [[nodiscard]] GizmoState& State() noexcept { return m_State; }
    [[nodiscard]] const GizmoState& State() const noexcept { return m_State; }

private:
    void ApplyStyleOnce();
    GizmoState m_State;
    bool m_StyleInitialized = false;
};

} // namespace Astral
