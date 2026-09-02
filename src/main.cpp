#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include <iostream>

// Fizik Sistemi: yalnizca verileri isler, kendi icinde durum tutmaz.
// SparseSet'in dense (kontigu) dizisini gezer -> cache dostu.
void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<Astral::TransformComponent>();

    // NOT: Proxy iterator prvalue dondurur; bu yuzden auto&& kullanilir.
    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<Astral::VelocityComponent>(entity)) {
            auto& velocity = registry.GetComponent<Astral::VelocityComponent>(entity);

            transform.x += velocity.dx * deltaTime;
            transform.y += velocity.dy * deltaTime;
            transform.z += velocity.dz * deltaTime;

            std::cout << "Gemi " << entity << " -> X: " << transform.x
                      << " | Y: " << transform.y << "\n";
        }
    }
}

int main() {
    Astral::Registry registry;

    // 1. Oyuncu gemisi (Transform + Velocity + Health)
    Astral::Entity playerShip = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(playerShip, {0.0f, 0.0f, 0.0f});
    registry.AddComponent<Astral::VelocityComponent>(playerShip, {15.0f, 5.0f, 0.0f});
    registry.AddComponent<Astral::HealthComponent>(playerShip, {200});

    // 2. Sabit uzay istasyonu (yalnizca Transform)
    Astral::Entity spaceStation = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(spaceStation, {100.0f, 100.0f, 0.0f});

    // 3. Goktasi (Transform + Velocity; hizi swap-and-pop ile kaldirilacak)
    Astral::Entity asteroid = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(asteroid, {0.0f, 50.0f, 0.0f});
    registry.AddComponent<Astral::VelocityComponent>(asteroid, {2.0f, 0.0f, 0.0f});

    // --- Health dogrulamasi: Type Erasure indeksleri karistirmyor mu? ---
    std::cout << "[Test] playerShip.hp    = "
              << registry.GetComponent<Astral::HealthComponent>(playerShip).hp << "\n";
    std::cout << "[Test] station has hp?  = "
              << (registry.HasComponent<Astral::HealthComponent>(spaceStation) ? "evet" : "hayir") << "\n";

    std::cout << "\n--- Kare 1 ---\n";
    PhysicsSystem(registry, 1.0f);

    // --- swap-and-pop: goktasi nin hizini kaldir ---
    std::cout << "\n[Test] RemoveComponent<Velocity>(asteroid)\n";
    const bool removed = registry.RemoveComponent<Astral::VelocityComponent>(asteroid);
    std::cout << "[Test] kaldi mi? -> " << (removed ? "evet" : "hayir") << "\n";
    std::cout << "[Test] HasComponent hala velocity? -> "
              << (registry.HasComponent<Astral::VelocityComponent>(asteroid) ? "evet" : "hayir") << "\n\n";

    for (int i = 2; i <= 3; i++) {
        std::cout << "--- Kare " << i << " ---\n";
        PhysicsSystem(registry, 1.0f);
        std::cout << "\n";
    }

    // --- DestroyEntity: istasyonu tum havuzlardan kaldir ---
    std::cout << "--- DestroyEntity(spaceStation) ---\n";
    registry.DestroyEntity(spaceStation);
    std::cout << "[Test] istasyon Transform hala var mi? -> "
              << (registry.HasComponent<Astral::TransformComponent>(spaceStation) ? "evet" : "hayir") << "\n";

    return 0;
}