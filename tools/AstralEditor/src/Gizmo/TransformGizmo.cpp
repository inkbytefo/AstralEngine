#include "Astral/Editor/Gizmo/TransformGizmo.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Scene/Entity.hpp"
#include "Astral/Scene/Scene.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Astral {

namespace {
ImGuizmo::OPERATION ToImGuizmoOperation(GizmoOperation operation) {
    switch (operation) {
        case GizmoOperation::Rotate: return ImGuizmo::ROTATE;
        case GizmoOperation::Scale: return ImGuizmo::SCALE;
        case GizmoOperation::Universal: return ImGuizmo::UNIVERSAL;
        default: return ImGuizmo::TRANSLATE;
    }
}
}

void TransformGizmo::UpdateShortcuts(const InputSystem& input, bool viewportActive, bool textInputActive) {
    if (!viewportActive || textInputActive) return;
    if (input.IsKeyJustPressed(GLFW_KEY_Q)) m_State.operation = GizmoOperation::Select;
    if (input.IsKeyJustPressed(GLFW_KEY_W)) m_State.operation = GizmoOperation::Translate;
    if (input.IsKeyJustPressed(GLFW_KEY_E)) m_State.operation = GizmoOperation::Rotate;
    if (input.IsKeyJustPressed(GLFW_KEY_R)) m_State.operation = GizmoOperation::Scale;
    if (input.IsKeyJustPressed(GLFW_KEY_T)) m_State.operation = GizmoOperation::Universal;
}

void TransformGizmo::ApplyStyleOnce() {
    if (m_StyleInitialized) return;
    auto& style = ImGuizmo::GetStyle();
    style.TranslationLineThickness = 3.0f;
    style.TranslationLineArrowSize = 7.0f;
    style.RotationLineThickness = 3.0f;
    style.RotationOuterLineThickness = 2.5f;
    style.ScaleLineThickness = 3.0f;
    style.ScaleLineCircleSize = 6.0f;
    style.CenterCircleSize = 6.0f;
    style.HatchedAxisLineThickness = 2.0f;
    style.Colors[ImGuizmo::DIRECTION_X] = ImVec4(0.92f, 0.25f, 0.28f, 1.0f);
    style.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.25f, 0.82f, 0.42f, 1.0f);
    style.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.25f, 0.55f, 0.95f, 1.0f);
    style.Colors[ImGuizmo::PLANE_X] = ImVec4(0.92f, 0.25f, 0.28f, 0.38f);
    style.Colors[ImGuizmo::PLANE_Y] = ImVec4(0.25f, 0.82f, 0.42f, 0.38f);
    style.Colors[ImGuizmo::PLANE_Z] = ImVec4(0.25f, 0.55f, 0.95f, 0.38f);
    style.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(0.25f, 0.55f, 0.95f, 0.25f);
    style.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.0f, 0.82f, 0.24f, 1.0f);
    m_StyleInitialized = true;
}

bool TransformGizmo::Manipulate(Scene& scene, Entity& entity, const glm::mat4& view,
                                const glm::mat4& projection, const GizmoViewportRect& viewport,
                                const InputSystem* input) {
    m_State.usingGizmo = false;
    m_State.hoveringGizmo = false;
    if (!m_State.IsEnabled() || !entity.IsValid() || !entity.HasComponent<TransformComponent>() ||
        viewport.width <= 0.0f || viewport.height <= 0.0f) return false;

    ApplyStyleOnce();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport.x, viewport.y, viewport.width, viewport.height);

    glm::mat4 worldTransform = scene.GetWorldTransform(entity.GetHandle());
    const bool snapEnabled = input && (input->IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                                       input->IsKeyPressed(GLFW_KEY_RIGHT_CONTROL));
    float snapValue = m_State.translationSnap;
    if (m_State.operation == GizmoOperation::Rotate) snapValue = m_State.rotationSnapDegrees;
    if (m_State.operation == GizmoOperation::Scale) snapValue = m_State.scaleSnap;
    float snapValues[3] = {snapValue, snapValue, snapValue};

    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
        ToImGuizmoOperation(m_State.operation),
        m_State.space == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
        glm::value_ptr(worldTransform), nullptr, snapEnabled ? snapValues : nullptr);

    m_State.usingGizmo = ImGuizmo::IsUsing();
    m_State.hoveringGizmo = ImGuizmo::IsOver();
    if (!m_State.usingGizmo) return false;

    glm::mat4 localTransform = worldTransform;
    const auto& registry = scene.GetRegistry();
    if (registry.HasComponent<HierarchyComponent>(entity.GetHandle())) {
        const EntityHandle parent = registry.GetComponent<HierarchyComponent>(entity.GetHandle()).parent;
        if (registry.IsAlive(parent)) localTransform = glm::inverse(scene.GetWorldTransform(parent)) * worldTransform;
    }

    auto& transform = entity.GetComponent<TransformComponent>();
    DecomposeTransformMatrix(localTransform, transform.position, transform.rotation, transform.scale);
    return true;
}

} // namespace Astral
