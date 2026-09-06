#pragma once

#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace Astral {

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float INV_PI = 0.31830988618379067154f;

/// 8-phase Halton(2, 3) low-discrepancy sequence in [-1, 1] pixel space (scale by 0.5 for subpixel jitter)
inline constexpr std::array<glm::vec2, 8> HALTON_SEQUENCE_8 = {{
    { 0.0f,        -0.333333f},
    {-0.5f,         0.333333f},
    { 0.5f,        -0.777778f},
    {-0.75f,       -0.111111f},
    { 0.25f,        0.555556f},
    {-0.25f,       -0.555556f},
    { 0.75f,        0.111111f},
    {-0.875f,       0.777778f}
}};

struct QualitySettings {
    // Primary Raymarching
    uint32_t primaryRayMaxSteps = 96;
    float rayHitEpsilon = 0.001f;

    // Shadows & Visibility
    uint32_t shadowMaxSteps = 96;
    float shadowMaxDistance = 50.0f;
    float shadowK = 24.0f;
    float surfaceBias = 0.015f;

    // Ambient Occlusion
    uint32_t aoSamples = 8;
    float aoRadius = 1.0f;

    // Spatial Acceleration Grid
    float gridCellSize = 0.75f;
    float gridMargin = 2.0f;
    glm::vec3 defaultGridMin{-12.0f, -1.0f, -12.0f};
    glm::vec3 defaultGridMax{ 12.0f, 11.0f,  12.0f};

    // Temporal Invalidation & TAA
    bool enableTAA = true;
    bool enableShadows = true;
    bool enableAO = true;
    float taaBlendAlpha = 0.12f;
    float screenDilation = 0.015f;
    float tonemapExposure = 1.0f;

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
