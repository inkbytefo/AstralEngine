#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <cmath>

namespace Astral::Test {

void RunPhysicsPipelineTests() {
    const std::string suite = "PhysicsPipelineSuite";

    Registry registry;
    const EntityHandle entity = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(entity, {});
    registry.AddComponent<VelocityComponent>(
        entity,
        {glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)}
    );
    registry.AddComponent<SDFComponent>(entity, {});

    PhysicsSubsystem physics;
    TEST_CHECK(suite, "PhysicsInitiallyEnabled", physics.IsEnabled());
    physics.SetEnabled(false);
    TEST_CHECK(suite, "PhysicsDisabled", !physics.IsEnabled());
    physics.SetEnabled(true);

    PhysicsSubsystem::Integrate(registry, 0.016f);
    TEST_CHECK(suite, "PositionXIntegrated",
               std::abs(registry.GetComponent<TransformComponent>(entity).position.x - 0.032f) < 0.0001f);
    TEST_CHECK(suite, "RotationNormalized",
               std::abs(glm::length(registry.GetComponent<TransformComponent>(entity).rotation) - 1.0f) < 0.0001f);

    UpdateWorldTransforms(registry);
    std::vector<SDFEditGPU> edits;
    std::vector<EntityHandle> entities;
    ExtractRenderData(registry, edits, entities);

    TEST_CHECK(suite, "ExtractionCountMatches", edits.size() == 1);
    TEST_CHECK(suite, "ExtractionEntityMatches", entities.size() == 1 && entities[0] == entity);
    TEST_CHECK_MSG(suite, "ExtractionPositionMatches",
                   std::abs(edits[0].position.x - 0.032f) < 0.0001f,
                   "Physics sonucu ayni karede world transform ve render extraction'a yansimali!");
}

} // namespace Astral::Test
