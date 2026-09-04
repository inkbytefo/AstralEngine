#pragma once

#include "Astral/Editor/Gizmo/GizmoState.hpp"
#include <glm/glm.hpp>

struct ImDrawList;

namespace Astral {

class ViewportGizmoToolbar {
public:
    void Draw(GizmoState& state, const glm::vec2& viewportOrigin,
              const glm::vec2& viewportSize, const glm::mat4& view);

private:
    void DrawOrientationIndicator(::ImDrawList* drawList, const glm::vec2& center,
                                  float radius, const glm::mat4& view) const;
};

} // namespace Astral
