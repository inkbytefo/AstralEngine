#include "Astral/Core/Application.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneManager.hpp"
#include <iostream>
#include <string>

// Fizik Sistemi: yalnizca verileri isler, kendi icinde durum tutmaz.
// SparseSet'in dense (kontigu) dizisini gezer -> cache dostu.
static void PhysicsSystem(Astral::Registry& registry, float deltaTime) {
    auto& transforms = registry.GetView<Astral::TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (registry.HasComponent<Astral::VelocityComponent>(entity)) {
            auto& velocity = registry.GetComponent<Astral::VelocityComponent>(entity);
            transform.position += velocity.linear * deltaTime;
        }
    }
}

static void RunEcsTests() {
    std::cout << "=== [Astral Engine: ECS Dogrulama Testi] ===\n";
    Astral::Registry registry;

    // 1. Oyuncu gemisi (Transform + Velocity + Health)
    Astral::EntityID playerShip = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(playerShip, {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<Astral::VelocityComponent>(playerShip, {{15.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    registry.AddComponent<Astral::HealthComponent>(playerShip, {200});

    // 2. Sabit uzay istasyonu (yalnizca Transform)
    Astral::EntityID spaceStation = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(spaceStation, {{100.0f, 100.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});

    // 3. Goktasi (Transform + Velocity)
    Astral::EntityID asteroid = registry.CreateEntity();
    registry.AddComponent<Astral::TransformComponent>(asteroid, {{0.0f, 50.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<Astral::VelocityComponent>(asteroid, {{2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});

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

static void RunSceneTests() {
    std::cout << "=== [Astral Engine: Scene Management & Deep-Copy Dogrulama Testi] ===\n";

    // 1. Editor Sahnesi olustur
    auto editorScene = std::make_shared<Astral::Scene>("Authoring Level");
    Astral::Entity originalShip = editorScene->CreateEntity();
    originalShip.AddComponent<Astral::TransformComponent>(glm::vec3(10.0f, 20.0f, 30.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    originalShip.AddComponent<Astral::VelocityComponent>(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
    originalShip.AddComponent<Astral::HealthComponent>(500);

    std::cout << "[Scene Test] Editor Seviyesi Hazirlandi. Nesne ID: " << originalShip.GetID()
              << ", Orijinal Pos X: " << originalShip.GetComponent<Astral::TransformComponent>().position.x
              << ", HP: " << originalShip.GetComponent<Astral::HealthComponent>().hp << "\n";

    // 2. Play dugmesine basildi: Editor -> Runtime Deep-Copy Klonlama
    auto runtimeScene = Astral::Scene::Copy(editorScene);
    assert(runtimeScene != nullptr);
    assert(runtimeScene != editorScene);

    Astral::SceneManager sceneManager;
    sceneManager.SetActiveScene(runtimeScene);
    runtimeScene->OnRuntimeStart();

    // 3. Runtime sahnesinde simulasyon calistir ve nesneleri mutasyona ugrat
    Astral::Entity clonedShip(originalShip.GetHandle(), runtimeScene.get());
    assert(clonedShip.IsValid());
    assert(clonedShip.HasComponent<Astral::TransformComponent>());

    runtimeScene->OnUpdate(2.0f); // 5.0 m/s * 2s = +10m -> pos.x = 20.0f
    clonedShip.GetComponent<Astral::HealthComponent>().hp = 120; // Can azaldi

    std::cout << "[Scene Test] Runtime Simulasyon Sonrasi: Cloned Pos X: " 
              << clonedShip.GetComponent<Astral::TransformComponent>().position.x
              << ", Cloned HP: " << clonedShip.GetComponent<Astral::HealthComponent>().hp << "\n";

    // 4. Orijinal Editor sahnesinin bozulmadigini (Derin kopyalamanin basarisini) teyit et!
    float origX = originalShip.GetComponent<Astral::TransformComponent>().position.x;
    int origHp = originalShip.GetComponent<Astral::HealthComponent>().hp;
    std::cout << "[Scene Test] Orijinal Editor Sahnesi Durumu: Pos X = " << origX << " (Beklenen: 10.0), HP = " << origHp << " (Beklenen: 500)\n";
    assert(origX == 10.0f && "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");
    assert(origHp == 500 && "Deep copy basarisiz! Editor sahnesi mutasyona ugradi!");

    // 5. Runtime sahnesinde nesneyi Destroy et
    runtimeScene->DestroyEntity(clonedShip);
    assert(!clonedShip.HasComponent<Astral::HealthComponent>());
    assert(originalShip.HasComponent<Astral::HealthComponent>() && "Editor nesnesi silinmemelidir!");

    sceneManager.UnloadCurrentScene();
    std::cout << "=== [Scene Management & Deep-Copy Dogrulama Testi Basariyla Tamamlandi] ===\n\n";
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
        } else if (arg == "--opt-shadow") {
            config.optShadow = true;
        } else if (arg == "--no-opt-shadow") {
            config.optShadow = false;
        } else if (arg == "--taa") {
            config.enableTAA = true;
        } else if (arg == "--no-taa") {
            config.enableTAA = false;
        }
    }

    // 1. ECS Cekirdek Testlerini Calistir
    RunEcsTests();

    // 2. Scene Management & Deep-Copy Testlerini Calistir
    RunSceneTests();

    // 3. Astral Engine Vulkan 1.4 & Pencere Uygulamasini Calistir
    Astral::Application app(config);
    app.Run(maxFrames);

    return 0;
}