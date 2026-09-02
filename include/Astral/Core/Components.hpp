#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

namespace Astral {

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    glm::vec3 scale{1.0f};
};

struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};

struct HealthComponent {
    int hp = 100;
};

struct SDFComponent {
    uint32_t primitiveType = 0; // 0: Sphere, 1: Box, 2: Torus, 3: Plane, 4: Capsule, 5: Cylinder
    uint32_t operation = 3;     // 0: Union, 1: Sub, 2: Intersect, 3: SmoothUnion, 4: SmoothSubtract
    float blendFactor = 0.25f;
    uint32_t isDynamic = 1;
    glm::vec3 albedo{1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
};

} // namespace Astral
