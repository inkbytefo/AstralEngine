#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <atomic>
#include <cmath>

namespace Astral {

static std::atomic<uint64_t> s_SnapshotRevisionCounter{1};

static bool IsEntityVisibleInHierarchy(const Registry& registry, EntityHandle entity) {
    if (!registry.IsAlive(entity)) return false;

    if (registry.HasComponent<VisibilityComponent>(entity)) {
        if (!registry.GetComponent<VisibilityComponent>(entity).isVisible) {
            return false;
        }
    }

    if (registry.HasComponent<SDFComponent>(entity)) {
        if (registry.GetComponent<SDFComponent>(entity).isVisible == 0) {
            return false;
        }
    }

    if (registry.HasComponent<HierarchyComponent>(entity)) {
        EntityHandle parent = registry.GetComponent<HierarchyComponent>(entity).parent;
        if (registry.IsAlive(parent)) {
            return IsEntityVisibleInHierarchy(registry, parent);
        }
    }
    return true;
}

struct RecordEntityPair {
    SDFPrimitiveRecord record;
    EntityHandle entity;
};

SDFSceneSnapshot SDFSceneSnapshot::Extract(const Registry& registry, uint32_t sceneInstanceId) {
    SDFSceneSnapshot snapshot;
    snapshot.m_Revision = s_SnapshotRevisionCounter.fetch_add(1, std::memory_order_relaxed);

    const auto& transforms = registry.GetView<TransformComponent>();
    std::vector<RecordEntityPair> pairs;
    pairs.reserve(transforms.Size());

    for (auto&& [entity, tr] : transforms) {
        if (!registry.HasComponent<SDFComponent>(entity)) continue;
        if (!IsEntityVisibleInHierarchy(registry, entity)) continue;

        const auto& sdf = registry.GetComponent<SDFComponent>(entity);

        const glm::mat4 worldM = GetWorldTransformMatrix(registry, entity);

        // Fast determinant singularity & non-invertible check
        const float det = glm::determinant(worldM);
        if (!std::isfinite(det) || std::abs(det) < 1e-7f) {
            continue; // Identically exclude singular/degenerate matrices
        }

        const glm::mat4 invM = glm::inverse(worldM);
        bool matrixValid = true;
        for (int c = 0; c < 4 && matrixValid; ++c) {
            for (int r = 0; r < 4; ++r) {
                if (!std::isfinite(invM[c][r])) {
                    matrixValid = false;
                    break;
                }
            }
        }
        if (!matrixValid) continue;

        // Conservative scale calculation: min singular value / column norm
        const float sx = glm::length(glm::vec3(worldM[0]));
        const float sy = glm::length(glm::vec3(worldM[1]));
        const float sz = glm::length(glm::vec3(worldM[2]));
        const float minScale = std::min({sx, sy, sz});
        if (!std::isfinite(minScale) || minScale < 1e-6f) continue;

        glm::vec4 dims = sdf.shape.dimensions;
        float conservativeScale = minScale;
        glm::mat4 finalInvM = invM;

        if (sdf.encoding == SDFShapeEncoding::LegacyPackedScale) {
            // Backward compatibility: legacy entities packed shape extents into transform scale
            const glm::vec3 trScale = tr.scale;
            switch (sdf.primitiveType) {
                case 0: { // Sphere / Non-uniform Ellipsoid
                    if (std::abs(trScale.x - trScale.y) < 1e-5f && std::abs(trScale.y - trScale.z) < 1e-5f) {
                        dims = glm::vec4(std::abs(trScale.x), 0.0f, 0.0f, 0.0f);
                        const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                        finalInvM = glm::inverse(rigidM);
                        conservativeScale = 1.0f;
                    } else {
                        // Non-uniform legacy ellipsoid: evaluated via affine invM and unit sphere
                        dims = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                        finalInvM = invM;
                        conservativeScale = minScale;
                    }
                    break;
                }
                case 1: { // Box: half extents is abs(scale)
                    dims = glm::vec4(glm::abs(trScale), 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
                case 2: { // Torus: scale.x = major, scale.y = minor
                    dims = glm::vec4(std::abs(trScale.x), std::abs(trScale.y), 0.0f, 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
                case 3: { // Plane: scale.y is offset
                    dims = glm::vec4(trScale.y, 0.0f, 0.0f, 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
                case 4: { // Capsule: scale.x = radius, scale.y = height
                    dims = glm::vec4(std::abs(trScale.x), std::abs(trScale.y), 0.0f, 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
                case 5: { // Cylinder: scale.x = radius, scale.y = half height
                    dims = glm::vec4(std::abs(trScale.x), std::abs(trScale.y), 0.0f, 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
                default: {
                    dims = glm::vec4(std::abs(trScale.x), 0.0f, 0.0f, 0.0f);
                    const glm::mat4 rigidM = glm::translate(glm::mat4(1.0f), tr.position) * glm::mat4_cast(tr.rotation);
                    finalInvM = glm::inverse(rigidM);
                    conservativeScale = 1.0f;
                    break;
                }
            }
        }

        const SDFSurfaceKey surfaceKey{
            .entityIndex = GetEntityIndex(entity),
            .entityGeneration = GetEntityGeneration(entity),
            .sceneInstanceId = sceneInstanceId,
            .subPrimitiveIndex = 0
        };

        SDFPrimitiveRecord rec;
        rec.invTransform = finalInvM;
        rec.dimensions = dims;
        rec.albedoRoughness = glm::vec4(sdf.albedo, sdf.roughness);
        rec.metallicParams = glm::vec4(sdf.metallic, sdf.blendFactor, conservativeScale, static_cast<float>(sdf.isDynamic));
        rec.primitiveType = sdf.primitiveType;
        rec.operation = sdf.operation;
        rec.csgOrder = static_cast<uint32_t>(sdf.csgOrder);
        rec.surfaceId = surfaceKey.Hash();

        pairs.push_back(RecordEntityPair{rec, entity});
    }

    // Deterministic ordering: strict sort by (csgOrder, surfaceId)
    std::sort(pairs.begin(), pairs.end(),
              [](const RecordEntityPair& a, const RecordEntityPair& b) {
                  if (a.record.csgOrder != b.record.csgOrder) return a.record.csgOrder < b.record.csgOrder;
                  return a.record.surfaceId < b.record.surfaceId;
              });

    snapshot.m_Records.reserve(pairs.size());
    snapshot.m_Entities.reserve(pairs.size());
    for (const auto& p : pairs) {
        snapshot.m_Records.push_back(p.record);
        snapshot.m_Entities.push_back(p.entity);
    }

    return snapshot;
}

Geometry::SDFHitResult SDFSceneSnapshot::Evaluate(const glm::vec3& worldPos) const {
    Geometry::SDFHitResult hit{};
    hit.distance = 1000.0f;
    hit.confidence = 1.0f;
    hit.surfaceId = 0u;
    hit.hitIndex = -1;
    hit.albedo = glm::vec3(0.5f);
    hit.roughness = 0.5f;
    hit.metallic = 0.0f;

    if (m_Records.empty()) return hit;

    for (size_t i = 0; i < m_Records.size(); ++i) {
        const auto& rec = m_Records[i];
        const glm::vec4 lp4 = rec.invTransform * glm::vec4(worldPos, 1.0f);
        const glm::vec3 lp = glm::vec3(lp4);
        const float localDist = Geometry::evalPrimitiveDistance(lp, rec.primitiveType, rec.dimensions);
        const float worldDist = localDist * rec.metallicParams.z; // Conservative distance scale

        Geometry::combinePrimitive(
            hit,
            i == 0,
            worldDist,
            glm::vec3(rec.albedoRoughness),
            rec.albedoRoughness.w,
            rec.metallicParams.x,
            rec.surfaceId,
            static_cast<int32_t>(i),
            rec.operation,
            rec.metallicParams.y
        );
    }
    return hit;
}

float SDFSceneSnapshot::EvaluateDistance(const glm::vec3& worldPos) const {
    if (m_Records.empty()) return 1000.0f;

    float dist = 1000.0f;
    for (size_t i = 0; i < m_Records.size(); ++i) {
        const auto& rec = m_Records[i];
        const glm::vec4 lp4 = rec.invTransform * glm::vec4(worldPos, 1.0f);
        const glm::vec3 lp = glm::vec3(lp4);
        const float localDist = Geometry::evalPrimitiveDistance(lp, rec.primitiveType, rec.dimensions);
        const float worldDist = localDist * rec.metallicParams.z;

        dist = Geometry::combineDistance(dist, i == 0, worldDist, rec.operation, rec.metallicParams.y);
    }
    return dist;
}

} // namespace Astral
