#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"

#include "Astral/Core/Components.hpp"

#include <glm/gtx/quaternion.hpp>

namespace Astral {

void PhysicsSubsystem::OnInit() {}

void PhysicsSubsystem::OnUpdate(FrameContext& context) {
    Integrate(context.registry, context.deltaTime);
}

void PhysicsSubsystem::OnShutdown() {}

void PhysicsSubsystem::Integrate(Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<VelocityComponent>(entity)) {
            const auto& velocity = registry.GetComponent<VelocityComponent>(entity);

            // Render interpolasyonu ve simulasyon durumu sozlesmesi:
            // Onceki konumu kaydet (eger bilesen yoksa ekle)
            if (registry.HasComponent<PreviousTransformComponent>(entity)) {
                auto& prev = registry.GetComponent<PreviousTransformComponent>(entity);
                prev.position = transform.position;
                prev.rotation = transform.rotation;
                prev.scale = transform.scale;
            } else {
                registry.AddComponent<PreviousTransformComponent>(entity, PreviousTransformComponent{
                    .position = transform.position,
                    .rotation = transform.rotation,
                    .scale = transform.scale
                });
            }

            transform.position += velocity.linear * deltaTime;

            if (glm::length(velocity.angular) > 0.0001f) {
                glm::quat deltaRot = glm::quat(velocity.angular * deltaTime);
                transform.rotation = glm::normalize(deltaRot * transform.rotation);
            }
        }
    }
}

} // namespace Astral
