#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"

namespace Astral::Test {

void RunEcsTests() {
    const std::string suite = "EcsSuite";

    Registry registry;

    RenderExtractionSubsystem extractionSubsystem;
    TEST_CHECK(suite, "InitialExtractionEmptyEdits", extractionSubsystem.GetLastExtractedEdits().empty());
    TEST_CHECK(suite, "InitialExtractionEmptyEntities", extractionSubsystem.GetLastExtractedEntities().empty());

    // 1. Oyuncu gemisi (Transform + Velocity + Health)
    EntityID playerShip = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(playerShip, {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<VelocityComponent>(playerShip, {{15.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    registry.AddComponent<HealthComponent>(playerShip, {200});

    // 2. Sabit uzay istasyonu (yalnizca Transform)
    EntityID spaceStation = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(spaceStation, {{100.0f, 100.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});

    // 3. Goktasi (Transform + Velocity)
    EntityID asteroid = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(asteroid, {{0.0f, 50.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    registry.AddComponent<VelocityComponent>(asteroid, {{2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});

    TEST_CHECK(suite, "PlayerShipHpMatches", registry.GetComponent<HealthComponent>(playerShip).hp == 200);
    TEST_CHECK(suite, "StationNoHp", !registry.HasComponent<HealthComponent>(spaceStation));

    PhysicsSubsystem::Integrate(registry, 1.0f);

    const bool removed = registry.RemoveComponent<VelocityComponent>(asteroid);
    TEST_CHECK(suite, "AsteroidVelocityRemoved", removed);
    TEST_CHECK(suite, "AsteroidVelocityGone", !registry.HasComponent<VelocityComponent>(asteroid));

    registry.DestroyEntity(spaceStation);
    TEST_CHECK(suite, "StationDestroyed", !registry.HasComponent<TransformComponent>(spaceStation));
    TEST_CHECK(suite, "StationNotAlive", !registry.IsAlive(spaceStation));
}

} // namespace Astral::Test
