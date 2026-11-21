#pragma once

// CPU mirrors of shader-side structs. Layouts are checked with static_asserts;
// change both sides together.

#include <glm/glm.hpp>
#include <cstdint>

namespace Astral::Render::Detail {

inline constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

struct FrameData { // shaders/include/Common.glsl, std140
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::mat4 invViewProj;
    glm::mat4 viewProjNoJitter;
    glm::mat4 prevViewProjNoJitter;
    glm::mat4 shadowViewProj;
    glm::vec4 cameraPos;
    glm::vec4 viewport;
    glm::vec4 jitter;
    glm::vec4 sunDirection;
    glm::vec4 sunColor;
    glm::vec4 shadowParams;
    glm::vec4 envParams;
    glm::vec4 zenithColor;
    glm::vec4 horizonColor;
    glm::vec4 groundColor;
    glm::uvec4 counts;
    glm::vec4 clip;
};
static_assert(sizeof(FrameData) == 640);

struct GpuInstance { // shaders/include/Scene.glsl, std430
    glm::mat4 model;
    glm::mat4 prevModel;
    glm::mat4 normalMatrix;
    uint32_t materialIndex;
    uint32_t objectId;
    uint32_t pad0 = 0;
    uint32_t pad1 = 0;
};
static_assert(sizeof(GpuInstance) == 208);

struct GpuMaterial {
    glm::vec4 baseColor;
    glm::vec4 emissiveNormalScale;
    float metallic;
    float roughness;
    uint32_t baseColorTexture;
    uint32_t normalTexture;
    uint32_t metallicRoughnessTexture;
    uint32_t emissiveTexture;
    uint32_t pad0 = 0;
    uint32_t pad1 = 0;
};
static_assert(sizeof(GpuMaterial) == 64);

struct GpuPointLight {
    glm::vec4 positionRange;
    glm::vec4 colorIntensity;
};
static_assert(sizeof(GpuPointLight) == 32);

struct GpuSdfShape { // shaders/include/Sdf.glsl, std430
    glm::vec4 positionBlend;
    glm::vec4 rotation;
    glm::vec4 size;
    glm::vec4 albedoRoughness;
    glm::vec4 emissiveMetallic;
    uint32_t primitive;
    uint32_t operation;
    uint32_t objectId;
    float boundRadius;
};
static_assert(sizeof(GpuSdfShape) == 96);

struct SdfVolumeParams { // std140
    glm::vec4 originVoxel;
    glm::uvec4 gridDim;
    glm::uvec4 atlasBricks;
    glm::vec4 metrics;
};
static_assert(sizeof(SdfVolumeParams) == 64);

} // namespace Astral::Render::Detail
