#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include <iostream>

// Mantık: Sadece verileri isleyen Fizik Sistemi
void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    auto& transforms = registry.GetTransforms();
    
    // Tüm konum bilesenlerini don
    for (auto& [entity, transform] : transforms) {
        // Eger bu entity'nin hizi da varsa, konumunu guncelle
        if (registry.HasVelocity(entity)) {
            auto& velocity = registry.GetVelocity(entity);
            
            transform.x += velocity.dx * deltaTime;
            transform.y += velocity.dy * deltaTime;
            transform.z += velocity.dz * deltaTime;
            
            std::cout << "Gemi " << entity << " Konumu -> X: " 
                      << transform.x << " | Y: " << transform.y << "\n";
        }
    }
}

int main() {
    Astral::Registry registry;

    // 1. Entity (Oyuncu Gemisi) yaratiliyor
    Astral::Entity playerShip = registry.CreateEntity();
    registry.AddTransform(playerShip, {0.0f, 0.0f, 0.0f});
    registry.AddVelocity(playerShip, {15.0f, 5.0f, 0.0f});

    // 2. Entity (Sabit Uzay Istasyonu - Hizi yok)
    Astral::Entity spaceStation = registry.CreateEntity();
    registry.AddTransform(spaceStation, {100.0f, 100.0f, 0.0f});

    std::cout << "[Motor] Simulasyon Basliyor...\n";

    // Oyun Dongusu (Game Loop) Simulasyonu (3 Kare/Frame)
    for(int i = 0; i < 3; i++) {
        std::cout << "\n--- Kare (Frame) " << i + 1 << " ---\n";
        PhysicsSystem(registry, 1.0f); // 1 saniyelik artis
    }

    return 0;
}