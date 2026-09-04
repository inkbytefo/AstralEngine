#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/Registry.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <unordered_set>

namespace Astral {

glm::mat4 GetLocalTransformMatrix(const TransformComponent& transform) {
    return glm::translate(glm::mat4(1.0f), transform.position) *
           glm::mat4_cast(transform.rotation) *
           glm::scale(glm::mat4(1.0f), transform.scale);
}

namespace {

glm::mat4 GetWorldTransformMatrixRecursive(
    const Registry& registry,
    EntityHandle entity,
    std::unordered_set<EntityHandle>& visiting) {
    if (!registry.IsAlive(entity) || !registry.HasComponent<TransformComponent>(entity)) {
        return glm::mat4(1.0f);
    }

    const glm::mat4 local = GetLocalTransformMatrix(registry.GetComponent<TransformComponent>(entity));
    if (!visiting.insert(entity).second) {
        return local;
    }

    glm::mat4 world = local;
    if (registry.HasComponent<HierarchyComponent>(entity)) {
        const EntityHandle parent = registry.GetComponent<HierarchyComponent>(entity).parent;
        if (registry.IsAlive(parent) && registry.HasComponent<TransformComponent>(parent)) {
            world = GetWorldTransformMatrixRecursive(registry, parent, visiting) * local;
        }
    }

    visiting.erase(entity);
    return world;
}

} // namespace

glm::mat4 GetWorldTransformMatrix(const Registry& registry, EntityHandle entity) {
    std::unordered_set<EntityHandle> visiting;
    return GetWorldTransformMatrixRecursive(registry, entity, visiting);
}

void UpdateWorldTransforms(Registry& registry) {
    auto& transforms = registry.GetView<TransformComponent>();
    for (auto&& [entity, transform] : transforms) {
        (void)transform;
        WorldTransformComponent worldTransform{GetWorldTransformMatrix(registry, entity)};
        registry.AddComponent<WorldTransformComponent>(entity, std::move(worldTransform));
    }
}

void DecomposeTransformMatrix(
    const glm::mat4& matrix,
    glm::vec3& position,
    glm::quat& rotation,
    glm::vec3& scale) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, scale, rotation, position, skew, perspective);
    rotation = glm::normalize(rotation);
}

} // namespace Astral
