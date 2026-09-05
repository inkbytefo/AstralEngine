#pragma once

#include "Astral/Core/EntityHandle.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Astral {

class Registry;
struct TransformComponent;
struct PreviousTransformComponent;

[[nodiscard]] glm::mat4 GetLocalTransformMatrix(const TransformComponent& transform);
[[nodiscard]] glm::mat4 GetWorldTransformMatrix(const Registry& registry, EntityHandle entity);
void UpdateWorldTransforms(Registry& registry);
void DecomposeTransformMatrix(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale);

/// Render interpolasyonu: Iki transform durumu arasinda alpha [0..1] ile lineer/slerp gecis yapar
[[nodiscard]] TransformComponent InterpolateTransform(const PreviousTransformComponent& prev, const TransformComponent& curr, float alpha) noexcept;
[[nodiscard]] TransformComponent InterpolateTransform(const TransformComponent& prev, const TransformComponent& curr, float alpha) noexcept;

} // namespace Astral
