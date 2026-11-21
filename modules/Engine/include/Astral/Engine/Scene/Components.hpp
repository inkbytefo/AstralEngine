#pragma once

// Built-in components. Components are plain data; systems (render sync, physics,
// transforms) read them every frame. Everything here except the runtime-only
// components is saved in scene files.

#include "Astral/Engine/Ecs/Registry.hpp"
#include "Astral/Render/RenderTypes.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace Astral {

// Stable identity, used by scene files and parent links.
struct IdComponent {
    uint64_t id = 0;
};

struct NameComponent {
    std::string name = "Entity";
};

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 Matrix() const;
    // Splits a TRS matrix (shear/perspective are dropped). Falls back to identity parts.
    static Transform FromMatrix(const glm::mat4& matrix);
    glm::vec3 Forward() const { return rotation * glm::vec3(0, 0, -1); }
    glm::vec3 Right() const { return rotation * glm::vec3(1, 0, 0); }
    glm::vec3 Up() const { return rotation * glm::vec3(0, 1, 0); }
};

// Optional parent: Transform is then relative to the parent.
struct Parent {
    Entity entity;
};

// Runtime-only: computed by the transform system every frame.
struct WorldTransform {
    glm::mat4 matrix{1.0f};
};

struct CameraComponent {
    float verticalFov = glm::radians(60.0f);
    float nearPlane = 0.05f;
    float farPlane = 500.0f;
    bool primary = true; // the first primary camera is used to render the game
};

struct MaterialParams {
    glm::vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emissive{0.0f};
};

// Draws a mesh asset: "builtin:cube", "builtin:sphere", "builtin:plane" or a glTF path.
struct MeshRenderer {
    std::string mesh = "builtin:cube";
    bool useAssetMaterials = true;  // false: `material` overrides every primitive
    MaterialParams material;
    bool castShadows = true;
};

// An SDF shape. Position/rotation come from the entity's world transform.
struct SdfShapeComponent {
    Render::SdfPrimitive primitive = Render::SdfPrimitive::Sphere;
    Render::SdfOperation operation = Render::SdfOperation::Union;
    glm::vec4 size{0.5f, 0.5f, 0.5f, 0.0f};
    float blend = 0.25f;
    int32_t order = 0;
    Render::SdfMaterial material;
    bool collision = true; // contributes to physics collision
};

// Directional lights shine along the entity's forward (-Z) axis.
struct LightComponent {
    Render::LightType type = Render::LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    bool castShadows = true;
};

enum class BodyType : uint32_t { Static = 0, Dynamic = 1, Kinematic = 2 };
enum class ColliderShape : uint32_t { Box = 0, Sphere = 1, Capsule = 2 };

struct RigidBody {
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.1f;
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    glm::vec3 initialVelocity{0.0f}; // applied when the body is created (spawned projectiles)
};

struct Collider {
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 size{0.5f};     // box: half extents; sphere: x = radius; capsule: x = radius, y = half height
    glm::vec3 offset{0.0f};
};

// Runtime-only: set by the physics system while a body exists.
struct PhysicsBodyId {
    uint32_t id = 0xFFFFFFFFu;
    glm::vec3 linearVelocity{0.0f};
};

} // namespace Astral
