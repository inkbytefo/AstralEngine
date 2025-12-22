#include <catch2/catch_test_macros.hpp>
#include "Subsystems/Scene/Scene.h"
#include "Subsystems/Scene/SceneSerializer.h"
#include "Subsystems/Scene/Entity.h"
#include "ECS/Components.h"
#include <filesystem>
#include <fstream>

using namespace AstralEngine;

TEST_CASE("Scene Serialization and Deserialization", "[SceneSerializer]") {
    Scene scene;
    
    // Create entities
    Entity entity1 = scene.CreateEntity("Entity1");
    auto& tc1 = entity1.GetComponent<TransformComponent>();
    tc1.position = {1.0f, 2.0f, 3.0f};

    Entity entity2 = scene.CreateEntity("Entity2");
    auto& tc2 = entity2.GetComponent<TransformComponent>();
    tc2.position = {4.0f, 5.0f, 6.0f};
    
    // Parent entity2 to entity1
    scene.ParentEntity(entity2, entity1);

    // Verify hierarchy
    // entity2 should be child of entity1
    REQUIRE(scene.Reg().valid(entity1));
    REQUIRE(scene.Reg().valid(entity2));
    
    // Check parent relationship using RelationshipComponent
    // Assuming RelationshipComponent is used for hierarchy
    // (You might need to adjust this based on your actual hierarchy implementation)
    // For now, let's just check if entities exist and have transforms

    // Serialize
    std::string filepath = "test_scene.json";
    SceneSerializer serializer(&scene);
    serializer.Serialize(filepath);

    REQUIRE(std::filesystem::exists(filepath));

    // Deserialize into new scene
    Scene newScene;
    SceneSerializer newSerializer(&newScene);
    bool result = newSerializer.Deserialize(filepath);
    
    REQUIRE(result == true);

    // Verify entities
    auto view = newScene.Reg().view<TagComponent>();
    int count = 0;
    Entity newEntity1;
    Entity newEntity2;

    for (auto e : view) {
        Entity entity = { e, &newScene };
        std::string tag = entity.GetComponent<TagComponent>().tag;
        if (tag == "Entity1") newEntity1 = entity;
        if (tag == "Entity2") newEntity2 = entity;
        count++;
    }

    REQUIRE(count == 2);
    REQUIRE((bool)newEntity1 == true);
    REQUIRE((bool)newEntity2 == true);

    // Verify transform
    auto& newTc1 = newEntity1.GetComponent<TransformComponent>();
    REQUIRE(newTc1.position.x == 1.0f);
    REQUIRE(newTc1.position.y == 2.0f);
    REQUIRE(newTc1.position.z == 3.0f);

    auto& newTc2 = newEntity2.GetComponent<TransformComponent>();
    REQUIRE(newTc2.position.x == 4.0f);

    // Verify hierarchy
    // Check if newEntity2 has RelationshipComponent and parent is newEntity1
    REQUIRE(newEntity2.HasComponent<RelationshipComponent>());
    auto& rc2 = newEntity2.GetComponent<RelationshipComponent>();
    REQUIRE(newScene.Reg().valid(rc2.Parent));
    
    Entity parentOf2 = { rc2.Parent, &newScene };
    // We compare UUIDs because Entity handles (IDs) might be different, but UUIDs are persistent
    REQUIRE(parentOf2.GetUUID() == newEntity1.GetUUID());

    // Cleanup
    std::filesystem::remove(filepath);
}
