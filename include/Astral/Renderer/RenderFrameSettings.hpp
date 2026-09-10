#pragma once

#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderCamera.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>

namespace Astral {

/// Explicit per-frame rendering inputs.
/// Decouples frame inputs from persistent renderer state.
struct RenderFrameSettings {
    std::optional<RenderCamera> camera;
    glm::vec2 jitter{0.0f};
    QualitySettings quality{};
    uint64_t frameId = 0;
    uint32_t width = 0, height = 0;
    uint32_t normalMode = 0;
    int debugMode = 0;
    int selectedHitIndex = -1;
    bool useGrid = true;
    bool optimizedShadows = true;
    bool taaEnabled = true;
    float exposure = 1.0f;
};

} // namespace Astral
