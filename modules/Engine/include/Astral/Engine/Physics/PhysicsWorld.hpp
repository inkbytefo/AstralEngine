#pragma once

#include "Astral/Engine/Ecs/Registry.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <optional>

namespace Astral {

class Scene;

struct RaycastHit {
    Entity entity;           // null when an SDF surface was hit
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
};

struct PhysicsSettings {
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    float sdfCollisionCellSize = 0.15f; // resolution of SDF collision meshes (metres)
    float sdfChunkSize = 8.0f;          // SDF collision is meshed per chunk of this size
};

// Rigid-body physics (Jolt). Bodies are created for entities with RigidBody + Collider
// when play starts. SDF shapes marked `collision` become static triangle meshes,
// re-meshed per chunk when they change.
class PhysicsWorld {
public:
    explicit PhysicsWorld(const PhysicsSettings& settings = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void Start(Scene& scene);
    void Stop(Scene& scene);
    bool IsRunning() const;

    // Kinematic bodies follow their transforms, then the simulation advances and
    // dynamic bodies write their transforms back.
    void Step(Scene& scene, float deltaTime);

    std::optional<RaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;

    void SetGravity(const glm::vec3& gravity);
    void AddImpulse(Scene& scene, Entity entity, const glm::vec3& impulse);
    void SetLinearVelocity(Scene& scene, Entity entity, const glm::vec3& velocity);
    glm::vec3 GetLinearVelocity(Scene& scene, Entity entity) const;

    uint32_t BodyCount() const;
    uint32_t SdfCollisionTriangles() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Astral
