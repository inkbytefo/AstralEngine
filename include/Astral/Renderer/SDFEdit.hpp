#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Astral {

enum class PrimitiveType : uint32_t {
    Sphere = 0,
    Box = 1,
    Torus = 2,
    Plane = 3,
    Capsule = 4,
    Cylinder = 5
};

enum class CSGOperation : uint32_t {
    Union = 0,
    Subtract = 1,
    Intersect = 2,
    SmoothUnion = 3,
    SmoothSubtract = 4
};

/// GLSL std430 hizalama kurallarina tam uyumlu 96-bayt GPU primitif yapisi.
/// RENDERER_ARCHITECTURE.md Bolum c.2.a semasi.
struct alignas(16) SDFEditGPU {
    glm::vec3 position{0.0f};
    float pad1 = 0.0f;

    glm::vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion

    glm::vec3 scale{1.0f};
    uint32_t primitiveType = 0;

    uint32_t operation = 3; // SmoothUnion varsayilan
    float blendFactor = 0.25f;
    uint32_t isDynamic = 0;
    float pad2 = 0.0f;

    glm::vec3 albedo{1.0f};
    float roughness = 0.5f;

    float metallic = 0.0f;
    float matPad1 = 0.0f;
    float matPad2 = 0.0f;
    float matPad3 = 0.0f;
};

static_assert(sizeof(SDFEditGPU) == 96, "SDFEditGPU struct boyutu tam olarak 96 bayt olmalidir!");

} // namespace Astral
