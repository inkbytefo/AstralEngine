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

void ExtractRenderData(Registry& registry, std::vector<SDFPrimitiveRecord>& outEdits, std::vector<EntityHandle>& outEntities) {
    auto snapshot = SDFSceneSnapshot::Extract(registry, 1);
    outEdits = snapshot.GetRecords();
    outEntities = snapshot.GetEntities();
}

void ExtractRenderData(Registry& registry, std::vector<SDFPrimitiveRecord>& outEdits) {
    std::vector<EntityHandle> unusedEntities;
    ExtractRenderData(registry, outEdits, unusedEntities);
}

void ExtractRenderData(Registry& registry, std::vector<LegacySDFEdit>& outEdits, std::vector<EntityHandle>& outEntities) {
    std::vector<SDFPrimitiveRecord> records;
    ExtractRenderData(registry, records, outEntities);
    outEdits.clear();
    outEdits.reserve(records.size());

    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];
        LegacySDFEdit e{};
        glm::mat4 m = glm::inverse(rec.invTransform);
        e.position = glm::vec3(m[3]);
        glm::quat q = glm::quat_cast(m);
        e.rotation = glm::vec4(q.x, q.y, q.z, q.w);
        glm::vec3 worldScale(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
        if (rec.primitiveType == 1) {
            e.scale = glm::vec3(rec.dimensions);
        } else if (worldScale.x > 1e-5f || worldScale.y > 1e-5f || worldScale.z > 1e-5f) {
            e.scale = worldScale;
        } else {
            e.scale = glm::vec3(rec.dimensions);
        }
        e.primitiveType = rec.primitiveType;
        e.operation = rec.operation;
        e.blendFactor = rec.metallicParams.y;
        e.isDynamic = static_cast<uint32_t>(rec.metallicParams.w);
        e.albedo = glm::vec3(rec.albedoRoughness);
        e.roughness = rec.albedoRoughness.w;
        e.metallic = rec.metallicParams.x;

        if (i < outEntities.size() && registry.IsAlive(outEntities[i]) &&
            registry.HasComponent<WorldTransformComponent>(outEntities[i])) {
            auto& history = registry.GetComponent<WorldTransformComponent>(outEntities[i]);
            e.SetPrevPosition(history.hasRenderHistory ? history.renderedPosition : e.position);
            e.prevRotation = history.hasRenderHistory ? history.renderedRotation : e.rotation;
            e.prevScale = glm::vec4(history.hasRenderHistory ? history.renderedScale : e.scale, 0.0f);

            history.renderedPosition = e.position;
            history.renderedRotation = e.rotation;
            history.renderedScale = e.scale;
            history.hasRenderHistory = true;
        } else {
            e.SetPrevPosition(e.position);
            e.prevRotation = e.rotation;
            e.prevScale = glm::vec4(e.scale, 0.0f);
        }

        outEdits.push_back(e);
    }
}

void ExtractRenderData(Registry& registry, std::vector<LegacySDFEdit>& outEdits) {
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
