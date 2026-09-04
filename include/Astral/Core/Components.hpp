#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Astral/Core/EntityHandle.hpp"

namespace Astral {

struct TagComponent {
    std::string tag = "Entity";
};

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    glm::vec3 scale{1.0f};
};

/// TransformSystem tarafindan her kare uretilen, serialize edilmeyen world-space cache.
struct WorldTransformComponent {
    glm::mat4 matrix{1.0f};
};

/// Parent-child baglantilari ayri tutulur; TransformComponent kontigu ve trivially-copyable kalir.
struct HierarchyComponent {
    EntityHandle parent = NullEntityHandle;
    std::vector<EntityHandle> children;
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
    uint32_t isVisible = 1;     // 1: Gorunur, 0: Gizli
};

struct VisibilityComponent {
    bool isVisible = true;
};

// Astral Araç Fiziği için Soft-Body Deformasyon Düğümü
struct SoftBodyNode {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float mass = 1.0f;
};

// Astral Araç Fiziği / Deforme Olabilir Gövde Bileşeni
struct SoftBodyComponent {
    std::vector<SoftBodyNode> nodes;
    float stiffness = 0.85f;
    float damping = 0.05f;
    float pressure = 1.0f;
};

// ============================================================================
// Component Traits & Compile-Time Identifiers for DOD Binary Serialization
// ============================================================================
namespace Detail {
    constexpr uint64_t FNV1a64(std::string_view str) {
        uint64_t hash = 14695981039346656037ull;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ull;
        }
        return hash;
    }
}

template <typename T>
struct ComponentTraits {
    static constexpr uint64_t TypeHash = 0;
    static constexpr const char* Name = "Unknown";
};

#define ASTRAL_REGISTER_COMPONENT_TRAIT(Type) \
    template <> \
    struct ComponentTraits<Type> { \
        static constexpr uint64_t TypeHash = Detail::FNV1a64(#Type); \
        static constexpr const char* Name = #Type; \
    }

ASTRAL_REGISTER_COMPONENT_TRAIT(TransformComponent);
ASTRAL_REGISTER_COMPONENT_TRAIT(HierarchyComponent);
ASTRAL_REGISTER_COMPONENT_TRAIT(VelocityComponent);
ASTRAL_REGISTER_COMPONENT_TRAIT(HealthComponent);
ASTRAL_REGISTER_COMPONENT_TRAIT(SDFComponent);
ASTRAL_REGISTER_COMPONENT_TRAIT(VisibilityComponent);

#undef ASTRAL_REGISTER_COMPONENT_TRAIT

} // namespace Astral
