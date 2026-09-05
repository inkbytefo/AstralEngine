#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Scene/SDFWorldQuery.hpp"
#include "Astral/Core/Components.hpp"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Astral {

namespace {

float SdSphere(const glm::vec3& p, float r) noexcept {
    return glm::length(p) - r;
}

float SdBox(const glm::vec3& p, const glm::vec3& b) noexcept {
    glm::vec3 q = glm::abs(p) - b;
    return glm::length(glm::max(q, 0.0f)) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float SdTorus(const glm::vec3& p, const glm::vec2& t) noexcept {
    glm::vec2 q(glm::length(glm::vec2(p.x, p.z)) - t.x, p.y);
    return glm::length(q) - t.y;
}

float SdPlane(const glm::vec3& p, const glm::vec3& n, float h) noexcept {
    return glm::dot(p, n) + h;
}

float SdCapsule(glm::vec3 p, float h, float r) noexcept {
    p.y -= std::clamp(p.y, 0.0f, h);
    return glm::length(p) - r;
}

float SdCylinder(const glm::vec3& p, float h, float r) noexcept {
    glm::vec2 d = glm::abs(glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y)) - glm::vec2(r, h);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, 0.0f));
}

float EvalPrimitive(const glm::vec3& p, const TransformComponent& tr, const SDFComponent& sdf) noexcept {
    glm::vec3 lp = p - tr.position;
    if (glm::dot(tr.rotation, tr.rotation) > 0.001f) {
        lp = glm::conjugate(tr.rotation) * lp;
    }

    switch (sdf.primitiveType) {
        case 0: return SdSphere(lp, tr.scale.x);
        case 1: return SdBox(lp, tr.scale);
        case 2: return SdTorus(lp, glm::vec2(tr.scale.x, tr.scale.y));
        case 3: return SdPlane(lp, glm::vec3(0.0f, 1.0f, 0.0f), tr.scale.y);
        case 4: return SdCapsule(lp, tr.scale.y, tr.scale.x);
        case 5: return SdCylinder(lp, tr.scale.y, tr.scale.x);
        default: return SdSphere(lp, tr.scale.x);
    }
}

bool IsVisible(const Registry& reg, EntityHandle entity) noexcept {
    if (reg.HasComponent<VisibilityComponent>(entity)) {
        if (!reg.GetComponent<VisibilityComponent>(entity).isVisible) return false;
    }
    if (reg.HasComponent<SDFComponent>(entity)) {
        if (reg.GetComponent<SDFComponent>(entity).isVisible == 0) return false;
    }
    return true;
}

} // namespace

float SDFWorldQuery::QueryDistance(const Registry& reg, const glm::vec3& point) {
    EntityHandle dummy = NullEntityHandle;
    return QueryDistance(reg, point, dummy);
}

float SDFWorldQuery::QueryDistance(const Registry& reg, const glm::vec3& point, EntityHandle& outClosestEntity) {
    const auto& view = reg.GetView<TransformComponent>();

    float resDist = 1000.0f;
    outClosestEntity = NullEntityHandle;
    bool first = true;

    for (auto&& [entity, tr] : view) {
        if (!reg.HasComponent<SDFComponent>(entity)) continue;
        if (!IsVisible(reg, entity)) continue;

        const auto& sdf = reg.GetComponent<SDFComponent>(entity);
        float d = EvalPrimitive(point, tr, sdf);

        if (first) {
            resDist = d;
            outClosestEntity = entity;
            first = false;
        } else {
            float k = std::max(sdf.blendFactor, 0.001f);
            float h = std::clamp(0.5f + 0.5f * (resDist - d) / k, 0.0f, 1.0f);

            switch (sdf.operation) {
                case 0: // Union
                    if (d < resDist) {
                        resDist = d;
                        outClosestEntity = entity;
                    }
                    break;
                case 1: // Subtract
                    resDist = std::max(resDist, -d);
                    break;
                case 2: // Intersect
                    if (d > resDist) {
                        resDist = d;
                        outClosestEntity = entity;
                    }
                    break;
                case 3: // Smooth Union
                    resDist = glm::mix(resDist, d, h) - k * h * (1.0f - h);
                    if (h > 0.5f) {
                        outClosestEntity = entity;
                    }
                    break;
                case 4: // Smooth Subtract
                    {
                        float hs = std::clamp(0.5f - 0.5f * (resDist + d) / k, 0.0f, 1.0f);
                        resDist = glm::mix(resDist, -d, hs) + k * hs * (1.0f - hs);
                    }
                    break;
                default:
                    if (d < resDist) {
                        resDist = d;
                        outClosestEntity = entity;
                    }
                    break;
            }
        }
    }

    return resDist;
}

glm::vec3 SDFWorldQuery::QueryNormal(const Registry& reg, const glm::vec3& point, float eps) {
    const float dx = QueryDistance(reg, point + glm::vec3(eps, 0.0f, 0.0f)) -
                     QueryDistance(reg, point - glm::vec3(eps, 0.0f, 0.0f));
    const float dy = QueryDistance(reg, point + glm::vec3(0.0f, eps, 0.0f)) -
                     QueryDistance(reg, point - glm::vec3(0.0f, eps, 0.0f));
    const float dz = QueryDistance(reg, point + glm::vec3(0.0f, 0.0f, eps)) -
                     QueryDistance(reg, point - glm::vec3(0.0f, 0.0f, eps));

    glm::vec3 n(dx, dy, dz);
    float len = glm::length(n);
    return len > 1e-6f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
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
