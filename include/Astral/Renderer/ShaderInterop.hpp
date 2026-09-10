#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>

namespace Astral {

// =============================================================================
// 1. Selection Data GPU Buffer
// =============================================================================
struct alignas(16) SelectionDataGPU {
    int32_t hitIndex = -1;
    int32_t pad0 = 0;
    int32_t pad1 = 0;
    int32_t pad2 = 0;
    glm::vec4 hitPoint{0.0f}; // xyz: hitPoint, w: hitDistance
};
static_assert(sizeof(SelectionDataGPU) == 32, "SelectionDataGPU boyutu 32 bayt olmalidir!");
static_assert(alignof(SelectionDataGPU) == 16, "SelectionDataGPU 16 bayt hizali olmalidir!");
static_assert(offsetof(SelectionDataGPU, hitPoint) == 16, "hitPoint offseti 16 olmalidir!");

// =============================================================================
// 2. SDF Push Constants (Geometry / G-Buffer Pass)
// =============================================================================
struct alignas(16) SDFPushConstants {
    glm::vec4 camPos;      // xyz: pos, w: near clip
    glm::vec4 camDir;      // xyz: dir, w: normalMode (0=central, 1=tetrahedron)
    glm::vec4 screenRes;   // x: width, y: height, z: editCount, w: useGrid (0=off, 1=on)
    glm::vec4 gridParams;  // x: dimX, y: dimY, z: maxSteps (96.0), w: cellSize
    glm::vec4 taaParams;   // x: jitterX, y: jitterY, z: taaEnabled, w: blendAlpha
    glm::vec4 mouseParams; // x: mouseX, y: mouseY, z: pickRequested (0/1), w: selectedHitIndex (-1 = none)
    glm::vec4 cameraRight; // xyz: world right, w: focal length in viewport-height units
    glm::vec4 cameraUp;    // xyz: world up, w: far clip; camPos.w carries near clip
};
static_assert(sizeof(SDFPushConstants) == 128, "SDFPushConstants boyutu 128 bayt olmalidir!");
static_assert(alignof(SDFPushConstants) == 16, "SDFPushConstants 16 bayt hizali olmalidir!");
static_assert(offsetof(SDFPushConstants, cameraRight) == 96, "cameraRight offseti 96 olmalidir!");
static_assert(offsetof(SDFPushConstants, cameraUp) == 112, "cameraUp offseti 112 olmalidir!");

// =============================================================================
// 3. TAA Push Constants (Resolve Pass)
// =============================================================================
struct alignas(16) TAAPushConstants {
    glm::vec4 screenRes; // x: width, y: height, z: frameIndex, w: blendAlpha
    glm::vec4 colorParams{1.0f, 0.0f, 0.0f, 0.0f}; // x: linear exposure multiplier, y: numChangedRects, z: isGlobalChange, w: reserved
    glm::vec4 changedRect0{0.0f}; // xy: minUV, zw: maxUV
    glm::vec4 changedRect1{0.0f}; // xy: minUV, zw: maxUV
    glm::vec4 changedRect2{0.0f}; // xy: minUV, zw: maxUV
    glm::vec4 changedRect3{0.0f}; // xy: minUV, zw: maxUV
};
static_assert(sizeof(TAAPushConstants) == 96, "TAAPushConstants boyutu 96 bayt olmalidir!");
static_assert(alignof(TAAPushConstants) == 16, "TAAPushConstants 16 bayt hizali olmalidir!");
static_assert(offsetof(TAAPushConstants, colorParams) == 16, "colorParams offseti 16 olmalidir!");
static_assert(offsetof(TAAPushConstants, changedRect0) == 32, "changedRect0 offseti 32 olmalidir!");

// =============================================================================
// 4. Camera UBO Data
// =============================================================================
struct alignas(16) CameraUBOData {
    glm::mat4 currViewProj{1.0f};
    glm::mat4 prevViewProj{1.0f};
    glm::vec4 prevCameraPosition{0.0f};
    glm::vec4 jitter{0.0f}; // current.xy, previous.zw in pixels
};
static_assert(sizeof(CameraUBOData) == 160, "CameraUBOData boyutu 160 bayt olmalidir!");
static_assert(alignof(CameraUBOData) == 16, "CameraUBOData 16 bayt hizali olmalidir!");
static_assert(offsetof(CameraUBOData, currViewProj) == 0, "currViewProj offseti 0 olmalidir!");
static_assert(offsetof(CameraUBOData, prevViewProj) == 64, "prevViewProj offseti 64 olmalidir!");
static_assert(offsetof(CameraUBOData, prevCameraPosition) == 128, "prevCameraPosition offseti 128 olmalidir!");
static_assert(offsetof(CameraUBOData, jitter) == 144, "jitter offseti 144 olmalidir!");

// =============================================================================
// 5. Debug Composite Push Constants
// =============================================================================
struct alignas(16) DebugCompositePushConstants {
    glm::vec4 screenRes; // x: width, y: height, z: debugMode, w: unused
};
static_assert(sizeof(DebugCompositePushConstants) == 16, "DebugCompositePushConstants boyutu 16 bayt olmalidir!");
static_assert(alignof(DebugCompositePushConstants) == 16, "DebugCompositePushConstants 16 bayt hizali olmalidir!");

// =============================================================================
// 6. Light GPU & Light Buffer Header
// =============================================================================
struct alignas(16) LightGPU {
    glm::vec4 position{0.0f};  // xyz: world pos, w: type (0 = Directional, 1 = Point)
    glm::vec4 direction{0.0f}; // xyz: normalized dir, w: intensity
    glm::vec4 color{1.0f};     // rgb: color, w: radius / range
};
static_assert(sizeof(LightGPU) == 48, "LightGPU boyutu 48 bayt olmalidir!");
static_assert(alignof(LightGPU) == 16, "LightGPU 16 bayt hizali olmalidir!");
static_assert(offsetof(LightGPU, position) == 0, "position offseti 0 olmalidir!");
static_assert(offsetof(LightGPU, direction) == 16, "direction offseti 16 olmalidir!");
static_assert(offsetof(LightGPU, color) == 32, "color offseti 32 olmalidir!");

struct alignas(16) LightBufferHeader {
    uint32_t lightCount = 0;
    uint32_t pad0 = 0;
    uint32_t pad1 = 0;
    uint32_t pad2 = 0;
};
static_assert(sizeof(LightBufferHeader) == 16, "LightBufferHeader boyutu 16 bayt olmalidir!");
static_assert(alignof(LightBufferHeader) == 16, "LightBufferHeader 16 bayt hizali olmalidir!");

// =============================================================================
// 7. Deferred Lighting Push Constants
// =============================================================================
struct alignas(16) DeferredLightingPushConstants {
    glm::vec4 camPos;         // xyz: camPos, w: maxMipLevel (orn. 5.0)
    glm::vec4 camDir;         // xyz: camDir, w: exposure (orn. 1.0)
    glm::vec4 screenRes;      // xy: resolution, z: iblIntensity (orn. 1.0), w: editCount
    glm::vec4 cameraRight;    // xyz: right, w: focal length
    glm::vec4 cameraUp;       // xyz: up, w: useGrid (0.0 or 1.0)
    glm::vec4 rayParams;      // xy: same pixel jitter as geometry pass, z: shadowMaxDistance (50.0), w: surfaceBias (0.015)
    glm::vec4 shadowAOParams; // x: shadowMaxSteps (96.0), y: shadowK (24.0), z: aoSamples (8.0), w: aoRadius (1.0)
    glm::vec4 qualityParams;  // x: optShadow (1.0), y: gridDimX (32.0), z: gridDimY (16.0), w: gridCellSize (0.75)
};
static_assert(sizeof(DeferredLightingPushConstants) == 128, "DeferredLightingPushConstants boyutu 128 bayt olmalidir!");
static_assert(alignof(DeferredLightingPushConstants) == 16, "DeferredLightingPushConstants 16 bayt hizali olmalidir!");
static_assert(offsetof(DeferredLightingPushConstants, shadowAOParams) == 96, "shadowAOParams offseti 96 olmalidir!");
static_assert(offsetof(DeferredLightingPushConstants, qualityParams) == 112, "qualityParams offseti 112 olmalidir!");

} // namespace Astral
