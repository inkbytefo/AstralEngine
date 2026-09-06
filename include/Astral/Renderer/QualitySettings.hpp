#pragma once

#include <cstdint>

namespace Astral {

struct QualitySettings {
    uint32_t shadowMaxSteps = 96;
    float shadowMaxDistance = 50.0f;
    float shadowK = 24.0f;
    uint32_t aoSamples = 8;
    float aoRadius = 1.0f;
    bool enableTAA = true;
    bool enableShadows = true;
    bool enableAO = true;

    // Debug Modlari:
    // 0: Final Shaded
    // 1: Surface Identity
    // 2: Geometry Revision
    // 3: Temporal Confidence
    // 4: Rejection Reason
    // 5: Changed Region Mask
    // 6: Shadow Visibility
    // 7: Ambient Occlusion
    int debugMode = 0;
};

} // namespace Astral
