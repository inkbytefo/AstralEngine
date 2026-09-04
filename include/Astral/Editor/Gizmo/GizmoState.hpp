#pragma once

namespace Astral {

enum class GizmoOperation : int {
    Select = -1,
    Translate = 0,
    Rotate = 1,
    Scale = 2,
    Universal = 3
};

enum class GizmoSpace : int {
    World = 0,
    Local = 1
};

struct GizmoState {
    GizmoOperation operation = GizmoOperation::Translate;
    GizmoSpace space = GizmoSpace::World;
    float translationSnap = 0.5f;
    float rotationSnapDegrees = 45.0f;
    float scaleSnap = 0.1f;
    bool usingGizmo = false;
    bool hoveringGizmo = false;

    [[nodiscard]] bool IsEnabled() const noexcept { return operation != GizmoOperation::Select; }
    [[nodiscard]] bool IsInteracting() const noexcept { return usingGizmo || hoveringGizmo; }
    void ToggleSpace() noexcept {
        space = space == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World;
    }
};

[[nodiscard]] constexpr const char* GizmoOperationName(GizmoOperation operation) noexcept {
    switch (operation) {
        case GizmoOperation::Translate: return "Tasi";
        case GizmoOperation::Rotate: return "Dondur";
        case GizmoOperation::Scale: return "Olcekle";
        case GizmoOperation::Universal: return "Evrensel";
        default: return "Secim";
    }
}

[[nodiscard]] constexpr const char* GizmoSpaceName(GizmoSpace space) noexcept {
    return space == GizmoSpace::World ? "World" : "Local";
}

} // namespace Astral
