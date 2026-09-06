#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Scene/SDFWorldQuery.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Core/Components.hpp"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Astral {

float SDFWorldQuery::QueryDistance(const SDFSceneSnapshot& snapshot, const glm::vec3& point) {
    return snapshot.EvaluateDistance(point);
}

glm::vec3 SDFWorldQuery::QueryNormal(const SDFSceneSnapshot& snapshot, const glm::vec3& point, float eps) {
    const float dx = snapshot.EvaluateDistance(point + glm::vec3(eps, 0.0f, 0.0f)) -
                     snapshot.EvaluateDistance(point - glm::vec3(eps, 0.0f, 0.0f));
    const float dy = snapshot.EvaluateDistance(point + glm::vec3(0.0f, eps, 0.0f)) -
                     snapshot.EvaluateDistance(point - glm::vec3(0.0f, eps, 0.0f));
    const float dz = snapshot.EvaluateDistance(point + glm::vec3(0.0f, 0.0f, eps)) -
                     snapshot.EvaluateDistance(point - glm::vec3(0.0f, 0.0f, eps));

    glm::vec3 n(dx, dy, dz);
    float len = glm::length(n);
    return len > 1e-6f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

float SDFWorldQuery::QueryDistance(const Registry& reg, const glm::vec3& point) {
    EntityHandle dummy = NullEntityHandle;
    return QueryDistance(reg, point, dummy);
}

float SDFWorldQuery::QueryDistance(const Registry& reg, const glm::vec3& point, EntityHandle& outClosestEntity) {
    const auto snapshot = SDFSceneSnapshot::Extract(reg);
    const auto hit = snapshot.Evaluate(point);
    outClosestEntity = NullEntityHandle;

    if (hit.surfaceId != 0) {
        for (auto&& [entity, tr] : reg.GetView<TransformComponent>()) {
            if (reg.HasComponent<SDFComponent>(entity)) {
                SDFSurfaceKey key{
                    .entityIndex = GetEntityIndex(entity),
                    .entityGeneration = GetEntityGeneration(entity),
                    .sceneInstanceId = 1,
                    .subPrimitiveIndex = 0
                };
                if (key.Hash() == hit.surfaceId) {
                    outClosestEntity = entity;
                    break;
                }
            }
        }
    }

    return hit.distance;
}

glm::vec3 SDFWorldQuery::QueryNormal(const Registry& reg, const glm::vec3& point, float eps) {
    const auto snapshot = SDFSceneSnapshot::Extract(reg);
    return QueryNormal(snapshot, point, eps);
}

bool SDFWorldQuery::Raycast(const Registry& reg, const glm::vec3& origin, const glm::vec3& direction,
                           float maxDistance, SDFRaycastHit& outHit, uint32_t maxSteps) {
    float dirLen = glm::length(direction);
    if (dirLen < 1e-6f) {
        outHit.hasHit = false;
        return false;
    }

    glm::vec3 rayDir = direction / dirLen;
    float t = 0.0f;

    for (uint32_t step = 0; step < maxSteps && t <= maxDistance; ++step) {
        glm::vec3 currentPos = origin + rayDir * t;
        EntityHandle hitEntity = NullEntityHandle;
        float dist = QueryDistance(reg, currentPos, hitEntity);

        if (dist < 0.003f) {
            outHit.hasHit = true;
            outHit.distance = t;
            outHit.hitPoint = currentPos;
            outHit.hitNormal = QueryNormal(reg, currentPos);
            outHit.hitEntity = hitEntity;
            return true;
        }

        t += std::max(dist, 0.003f);
    }

    outHit.hasHit = false;
    return false;
}

bool SDFWorldQuery::ResolveSphereCollision(const Registry& reg, glm::vec3& position, float radius,
                                          float skin, uint32_t maxIterations) {
    bool resolved = false;

    for (uint32_t iter = 0; iter < maxIterations; ++iter) {
        float dist = QueryDistance(reg, position);
        if (dist < radius) {
            float penetration = radius - dist;
            glm::vec3 normal = QueryNormal(reg, position);
            position += normal * (penetration + skin);
            resolved = true;
        } else {
            break;
        }
    }

    return resolved;
}

// --- Scene overloads ---
float SDFWorldQuery::QueryDistance(const Scene& scene, const glm::vec3& point) {
    return QueryDistance(scene.GetRegistry(), point);
}

float SDFWorldQuery::QueryDistance(const Scene& scene, const glm::vec3& point, EntityHandle& outClosestEntity) {
    return QueryDistance(scene.GetRegistry(), point, outClosestEntity);
}

glm::vec3 SDFWorldQuery::QueryNormal(const Scene& scene, const glm::vec3& point, float eps) {
    return QueryNormal(scene.GetRegistry(), point, eps);
}

bool SDFWorldQuery::Raycast(const Scene& scene, const glm::vec3& origin, const glm::vec3& direction,
                           float maxDistance, SDFRaycastHit& outHit, uint32_t maxSteps) {
    return Raycast(scene.GetRegistry(), origin, direction, maxDistance, outHit, maxSteps);
}

bool SDFWorldQuery::ResolveSphereCollision(const Scene& scene, glm::vec3& position, float radius,
                                          float skin, uint32_t maxIterations) {
    return ResolveSphereCollision(scene.GetRegistry(), position, radius, skin, maxIterations);
}

} // namespace Astral
