#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"

#include "Astral/Core/Components.hpp"

#include <glm/gtx/quaternion.hpp>

namespace Astral {

void PhysicsSubsystem::OnInit() {}

void PhysicsSubsystem::OnUpdate(FrameContext& context) {
    // Physics bilincli olarak fixed timestep kullanir; paylasilan gercek frame delta'sini degistirmez.
    Integrate(context.registry, FixedTimeStep);
}

void PhysicsSubsystem::OnShutdown() {}

void PhysicsSubsystem::Integrate(Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<VelocityComponent>(entity)) {
            const auto& velocity = registry.GetComponent<VelocityComponent>(entity);

            transform.position += velocity.linear * deltaTime;

            if (glm::length(velocity.angular) > 0.0001f) {
                glm::quat deltaRot = glm::quat(velocity.angular * deltaTime);
                transform.rotation = glm::normalize(deltaRot * transform.rotation);
            }
        }
    }
}

} // namespace Astral
