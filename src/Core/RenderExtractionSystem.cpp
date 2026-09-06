#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>


namespace Astral {

bool SetActiveCamera(Registry& registry, EntityHandle camera) {
    if (camera != NullEntityHandle &&
        (!registry.HasComponent<CameraComponent>(camera) ||
         !registry.HasComponent<TransformComponent>(camera))) return false;
    for (auto&& [entity, component] : registry.GetView<CameraComponent>()) {
        component.primary = entity == camera ? 1u : 0u;
    }
    return true;
}

std::optional<RenderCamera> ExtractActiveCamera(Registry& registry, float aspect) {
    if (!std::isfinite(aspect) || aspect <= 0.0f) return std::nullopt;
    EntityHandle selected = NullEntityHandle;
    const CameraComponent* lens = nullptr;
    for (auto&& [entity, camera] : registry.GetView<CameraComponent>()) {
        if (camera.primary && registry.IsAlive(entity) &&
            registry.HasComponent<TransformComponent>(entity) &&
            (!lens || entity < selected)) {
            selected = entity;
            lens = &camera;
        }
    }
    if (!lens || !std::isfinite(lens->verticalFovRadians) ||
        !std::isfinite(lens->nearClip) || !std::isfinite(lens->farClip) ||
        lens->verticalFovRadians <= 0.001f || lens->verticalFovRadians >= glm::pi<float>() - 0.001f ||
        lens->nearClip <= 0.0f || lens->farClip <= lens->nearClip) return std::nullopt;

    const glm::mat4 world = registry.HasComponent<WorldTransformComponent>(selected)
        ? registry.GetComponent<WorldTransformComponent>(selected).matrix
        : GetWorldTransformMatrix(registry, selected);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(world[c][r])) return std::nullopt;
    const glm::vec3 forward = -glm::vec3(world[2]);
    const glm::vec3 right = glm::cross(forward, glm::vec3(world[1]));
    const float forwardLengthSquared = glm::dot(forward, forward);
    const float rightLengthSquared = glm::dot(right, right);
    if (!std::isfinite(forwardLengthSquared) || !std::isfinite(rightLengthSquared) ||
        forwardLengthSquared < 1e-12f || rightLengthSquared < 1e-12f) return std::nullopt;
    RenderCamera result;
    result.entity = selected;
    result.position = glm::vec3(world[3]);
    result.forward = glm::normalize(forward);
    result.right = glm::normalize(right);
    result.up = glm::cross(result.right, result.forward);
    result.nearClip = lens->nearClip;
    result.farClip = lens->farClip;
    result.view = glm::lookAt(result.position, result.position + result.forward, result.up);
    result.projection = glm::perspective(lens->verticalFovRadians, aspect, lens->nearClip, lens->farClip);
    return result;
}

namespace {

bool IsEntityVisibleInHierarchy(const Registry& registry, EntityHandle entity) {
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

} // namespace

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits, std::vector<EntityHandle>& outEntities) {
    outEdits.clear();
    outEntities.clear();
    outEdits.reserve(MAX_SDF_EDITS);
    outEntities.reserve(MAX_SDF_EDITS);

    auto& transforms = registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        (void)transform;
        if (registry.HasComponent<SDFComponent>(entity)) {
            // Görünürlük kontrolü: Nesne veya ebeveyni gizlendiyse çizilmez ve seçilmez
            if (!IsEntityVisibleInHierarchy(registry, entity)) {
                continue;
            }

            const auto& sdf = registry.GetComponent<SDFComponent>(entity);

            if (!registry.HasComponent<WorldTransformComponent>(entity)) {
                continue;
            }

            const glm::mat4& worldMatrix = registry.GetComponent<WorldTransformComponent>(entity).matrix;
            glm::vec3 worldPosition;
            glm::quat worldRotation;
            glm::vec3 worldScale;
            DecomposeTransformMatrix(worldMatrix, worldPosition, worldRotation, worldScale);

            SDFEditGPU gpuData{};
            gpuData.position = worldPosition;
            gpuData.rotation = glm::vec4(
                worldRotation.x,
                worldRotation.y,
                worldRotation.z,
                worldRotation.w
            );
            glm::vec3 effectiveScale = worldScale;
            if (sdf.encoding == SDFShapeEncoding::ExplicitShape) {
                switch (sdf.primitiveType) {
                    case 0: // Sphere: radius in x
                        effectiveScale = glm::vec3(sdf.shape.dimensions.x) * worldScale;
                        break;
                    case 1: // Box: half-extents in xyz
                        effectiveScale = glm::vec3(sdf.shape.dimensions.x, sdf.shape.dimensions.y, sdf.shape.dimensions.z) * worldScale;
                        break;
                    case 2: // Torus: major in x, minor in y
                        effectiveScale = glm::vec3(sdf.shape.dimensions.x, sdf.shape.dimensions.y, worldScale.z);
                        break;
                    case 3: // Plane: offset in x
                        effectiveScale = worldScale;
                        break;
                    case 4: // Capsule: radius in x, length in y
                    case 5: // Cylinder: radius in x, height in y
                        effectiveScale = glm::vec3(sdf.shape.dimensions.x, sdf.shape.dimensions.y, sdf.shape.dimensions.x) * worldScale;
                        break;
                    default:
                        effectiveScale = glm::vec3(sdf.shape.dimensions) * worldScale;
                        break;
                }
            }
            gpuData.scale = effectiveScale;
            gpuData.primitiveType = sdf.primitiveType;
            gpuData.operation = sdf.operation;
            gpuData.blendFactor = sdf.blendFactor;
            gpuData.isDynamic = sdf.isDynamic;
            gpuData.albedo = sdf.albedo;
            gpuData.roughness = sdf.roughness;
            gpuData.metallic = sdf.metallic;

            auto& history = registry.GetComponent<WorldTransformComponent>(entity);
            gpuData.SetPrevPosition(history.hasRenderHistory ? history.renderedPosition : worldPosition);
            gpuData.prevRotation = history.hasRenderHistory ? history.renderedRotation : gpuData.rotation;
            gpuData.prevScale = glm::vec4(history.hasRenderHistory ? history.renderedScale : effectiveScale, 0.0f);
            history.renderedPosition = worldPosition;
            history.renderedRotation = gpuData.rotation;
            history.renderedScale = effectiveScale;
            history.hasRenderHistory = true;
            outEdits.push_back(gpuData);
            outEntities.push_back(entity);
        }
    }

    // Deterministic ordering: sort edits by csgOrder, tie-break with entity handle
    if (outEdits.size() > 1) {
        struct EditEntityPair {
            SDFEditGPU edit;
            EntityHandle entity;
            uint64_t csgOrder;
        };
        std::vector<EditEntityPair> pairs;
        pairs.reserve(outEdits.size());
        for (size_t i = 0; i < outEdits.size(); ++i) {
            uint64_t csg = registry.HasComponent<SDFComponent>(outEntities[i])
                ? registry.GetComponent<SDFComponent>(outEntities[i]).csgOrder
                : 0;
            pairs.push_back({outEdits[i], outEntities[i], csg});
        }
        std::sort(pairs.begin(), pairs.end(), [](const EditEntityPair& a, const EditEntityPair& b) {
            if (a.csgOrder != b.csgOrder) return a.csgOrder < b.csgOrder;
            return a.entity < b.entity;
        });
        for (size_t i = 0; i < pairs.size(); ++i) {
            outEdits[i] = pairs[i].edit;
            outEntities[i] = pairs[i].entity;
        }
    }
}

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits) {
    std::vector<EntityHandle> unusedEntities;
    ExtractRenderData(registry, outEdits, unusedEntities);
}

void ExtractAndUploadRenderData(Registry& registry, void* mappedGpuBuffer, uint32_t& outEditCount) {
    if (!mappedGpuBuffer) {
        outEditCount = 0;
        return;
    }

    auto snapshot = SDFSceneSnapshot::Extract(registry, 1);
    outEditCount = std::min(snapshot.GetRecordCount(), static_cast<uint32_t>(MAX_SDF_EDITS));
    if (outEditCount > 0) {
        std::memcpy(mappedGpuBuffer, snapshot.GetRecords().data(), outEditCount * sizeof(SDFPrimitiveRecord));
    }
}

} // namespace Astral
