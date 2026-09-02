#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include <iostream>
#include <string>

// Fizik Sistemi: yalnizca verileri isler, kendi icinde durum tutmaz.
// SparseSet'in dense (kontigu) dizisini gezer -> cache dostu.
static void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<Astral::TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<Astral::VelocityComponent>(entity)) {
            auto& velocity = registry.GetComponent<Astral::VelocityComponent>(entity);

            transform.x += velocity.dx * deltaTime;
            transform.y += velocity.dy * deltaTime;
            transform.z += velocity.dz * deltaTime;
        }
    }
}

static void RunEcsTests() {
    std::cout << "=== [Astral Engine: ECS Dogrulama Testi] ===\n";
    Astral::Registry registry;

    // 1. Oyuncu gemisi (Transform + Velocity + Health)
    Astral::Entity playerShip = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(playerShip, {0.0f, 0.0f, 0.0f});
    registry.AddComponent<Astral::VelocityComponent>(playerShip, {15.0f, 5.0f, 0.0f});
    registry.AddComponent<Astral::HealthComponent>(playerShip, {200});

    // 2. Sabit uzay istasyonu (yalnizca Transform)
    Astral::Entity spaceStation = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(spaceStation, {100.0f, 100.0f, 0.0f});

    // 3. Goktasi (Transform + Velocity)
    Astral::Entity asteroid = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(asteroid, {0.0f, 50.0f, 0.0f});
    registry.AddComponent<Astral::VelocityComponent>(asteroid, {2.0f, 0.0f, 0.0f});

    std::cout << "[ECS Test] playerShip.hp = "
              << registry.GetComponent<Astral::HealthComponent>(playerShip).hp << "\n";
    std::cout << "[ECS Test] station has hp? = "
              << (registry.HasComponent<Astral::HealthComponent>(spaceStation) ? "evet" : "hayir") << "\n";

    PhysicsSystem(registry, 1.0f);

    const bool removed = registry.RemoveComponent<Astral::VelocityComponent>(asteroid);
    std::cout << "[ECS Test] Goktasi hizi kaldirildi mi? -> " << (removed ? "evet" : "hayir") << "\n";

    registry.DestroyEntity(spaceStation);
    std::cout << "[ECS Test] Istasyon DestroyEntity sonrasi var mi? -> "
              << (registry.HasComponent<Astral::TransformComponent>(spaceStation) ? "evet" : "hayir") << "\n";
    std::cout << "=== [ECS Dogrulama Testi Basariyla Tamamlandi] ===\n\n";
}

int main(int argc, char* argv[]) {
    Astral::AppConfig config;
    int maxFrames = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--test-only") {
            maxFrames = 10;
        } else if (arg == "--bench") {
            config.benchMode = true;
        } else if (arg == "--bench-frames" && i + 1 < argc) {
            config.benchFrames = std::stoi(argv[++i]);
            config.benchMode = true;
        } else if (arg == "--bench-out" && i + 1 < argc) {
            config.benchOutputFile = argv[++i];
            config.benchMode = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::stoi(argv[++i]);
        } else if (arg == "--normal" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "tetra" || mode == "tetrahedron" || mode == "1") {
                config.normalMode = 1;
            } else {
                config.normalMode = 0;
            }
        } else if (arg == "--shader" && i + 1 < argc) {
            config.shaderPath = argv[++i];
        } else if (arg == "--legacy-map") {
            config.legacyMap = true;
        } else if (arg == "--grid") {
            config.useGrid = true;
        } else if (arg == "--no-grid") {
            config.useGrid = false;
        } else if (arg == "--stress") {
            config.stressTest = true;
        }
    }

    // 1. ECS Cekirdek Testlerini Calistir
    RunEcsTests();

    // 2. Astral Engine Vulkan 1.4 & Pencere Uygulamasini Calistir
    Astral::Application app(config);
    app.Run(maxFrames);

    return 0;
}