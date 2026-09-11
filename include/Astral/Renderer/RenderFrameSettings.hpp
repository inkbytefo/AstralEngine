#pragma once

#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderCamera.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>

namespace Astral {

/// Explicit per-frame rendering inputs.
/// Decouples frame inputs from persistent renderer state.
/// G12: ResolveFrameSettings resolves these inputs into the effective settings consumed by passes.
/// Inputs must satisfy width > 0, height > 0, and finite positive exposure (> 0.0f); safe fallbacks are applied otherwise.
struct RenderFrameSettings {
    std::optional<RenderCamera> camera;
    glm::vec2 jitter{0.0f};

    /// Resolved effective quality settings (populated by ResolveFrameSettings).
    QualitySettings quality{};

    /// Optional explicit quality override. If std::nullopt, the renderer's default quality is used.
    std::optional<QualitySettings> qualityOverride{std::nullopt};

    uint64_t frameId = 0;
    uint32_t width = 0, height = 0;
    uint32_t normalMode = 0;
    int debugMode = 0;
    int selectedHitIndex = -1;
    bool useGrid = true;
    bool optimizedShadows = true;
    bool taaEnabled = true;
    float exposure = 1.0f;
    uint32_t activePrimitiveCount = 0;
    glm::vec4 gridParams{0.0f};
    int pickMouseX = -1;
    int pickMouseY = -1;
    float pickFlag = 0.0f;
};

} // namespace Astral
