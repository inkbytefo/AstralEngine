#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Scene/Scene.hpp"
#include "Astral/Core/Components.hpp"
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace Astral {

void Scene::OnRuntimeStart() {
    m_IsRunning = true;
    std::cout << "[Astral::Scene] '" << m_Name << "' Runtime baslatildi.\n";
}

void Scene::OnUpdate(float deltaTime) {
    if (!m_IsRunning) return;

    // Fizik ve Kinematik Simülasyonu (Contiguous SparseSet Traversal)
    auto& transforms = m_Registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (m_Registry.HasComponent<VelocityComponent>(entity)) {
            const auto& velocity = m_Registry.GetComponent<VelocityComponent>(entity);

            // Çizgisel öteleme
            transform.position += velocity.linear * deltaTime;

            // Açısal rotasyon (Euler -> Quaternion integration)
            if (glm::length(velocity.angular) > 0.0001f) {
                glm::quat deltaRot = glm::quat(velocity.angular * deltaTime);
                transform.rotation = glm::normalize(deltaRot * transform.rotation);
            }
        }
    }
}

void Scene::OnRuntimeStop() {
    m_IsRunning = false;
    std::cout << "[Astral::Scene] '" << m_Name << "' Runtime durduruldu.\n";
}

} // namespace Astral
