#ifndef ASTRAL_SCENE_GLSL
#define ASTRAL_SCENE_GLSL

// GPU mirrors of Detail::GpuInstance / GpuMaterial / GpuLight (std430).
struct Instance {
    mat4 model;
    mat4 prevModel;
    mat4 normalMatrix;   // inverse-transpose of model (upper 3x3 used)
    uint materialIndex;
    uint objectId;
    uint pad0;
    uint pad1;
};

struct Material {
    vec4 baseColor;
    vec4 emissiveNormalScale;   // rgb emissive, w normal scale
    float metallic;
    float roughness;
    uint baseColorTexture;      // bindless slots, INVALID_INDEX if unused
    uint normalTexture;
    uint metallicRoughnessTexture;
    uint emissiveTexture;
    uint pad0;
    uint pad1;
};

struct PointLight {
    vec4 positionRange;  // xyz position, w range
    vec4 colorIntensity; // rgb color, w intensity
};

#endif
