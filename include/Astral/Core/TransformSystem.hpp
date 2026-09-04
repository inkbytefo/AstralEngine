#pragma once

#include "Astral/Core/EntityHandle.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Astral {

class Registry;
struct TransformComponent;

[[nodiscard]] glm::mat4 GetLocalTransformMatrix(const TransformComponent& transform);
[[nodiscard]] glm::mat4 GetWorldTransformMatrix(const Registry& registry, EntityHandle entity);
void UpdateWorldTransforms(Registry& registry);
void DecomposeTransformMatrix(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale);

} // namespace Astral
