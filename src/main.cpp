#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include <iostream>

void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    // Sadece Transform bilesenlerini cek
    auto& transforms = registry.GetView<Astral::TransformComponent>();

    for (auto& [entity, transform] : transforms) {
        if (registry.HasComponent<Astral::VelocityComponent>(entity)) {
            auto& velocity = registry.GetComponent<Astral::VelocityComponent>(entity);

            transform.x += velocity.dx * deltaTime;
            transform.y += velocity.dy * deltaTime;
            transform.z += velocity.dz * deltaTime;

            std::cout << "Gemi " << entity << " -> X: " << transform.x << " | Y: " << transform.y << "\n";
        }
    }
}

int main() {
    Astral::Registry registry;

    Astral::Entity playerShip = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(playerShip, {0.0f, 0.0f, 0.0f});
    registry.AddComponent<Astral::VelocityComponent>(playerShip, {15.0f, 5.0f, 0.0f});
    registry.AddComponent<Astral::HealthComponent>(playerShip, {200}); // Yeni bilesen basariyla eklendi

    Astral::Entity spaceStation = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(spaceStation, {100.0f, 100.0f, 0.0f});

    for(int i = 0; i < 3; i++) {
        std::cout << "\n--- Kare " << i + 1 << " ---\n";
        PhysicsSystem(registry, 1.0f);
    }

    return 0;
}