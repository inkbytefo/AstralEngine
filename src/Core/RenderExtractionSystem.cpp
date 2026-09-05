#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <unordered_map>

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
            gpuData.scale = worldScale;
            gpuData.primitiveType = sdf.primitiveType;
            gpuData.operation = sdf.operation;
            gpuData.blendFactor = sdf.blendFactor;
            gpuData.isDynamic = sdf.isDynamic;
            gpuData.albedo = sdf.albedo;
            gpuData.roughness = sdf.roughness;
            gpuData.metallic = sdf.metallic;

            static std::unordered_map<EntityHandle, glm::vec3> s_PrevEntityPositions;
            auto it = s_PrevEntityPositions.find(entity);
            if (it != s_PrevEntityPositions.end()) {
                gpuData.SetPrevPosition(it->second);
            } else {
                gpuData.SetPrevPosition(worldPosition);
            }
            s_PrevEntityPositions[entity] = worldPosition;

            outEdits.push_back(gpuData);
            outEntities.push_back(entity);
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

    std::vector<SDFEditGPU> uploadBuffer;
    ExtractRenderData(registry, uploadBuffer);

    outEditCount = static_cast<uint32_t>(uploadBuffer.size());
    if (outEditCount > 0) {
        std::memcpy(mappedGpuBuffer, uploadBuffer.data(), outEditCount * sizeof(SDFEditGPU));
    }
}

} // namespace Astral
