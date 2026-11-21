#include <doctest/doctest.h>

#include "Astral/Engine/Physics/PhysicsWorld.hpp"
#include "Astral/Engine/Scene/Scene.hpp"

using namespace Astral;

namespace {

Entity AddBox(Scene& scene, const char* name, glm::vec3 position, glm::vec3 halfExtents, BodyType type) {
    Entity e = scene.CreateEntity(name);
    auto& reg = scene.GetRegistry();
    reg.Get<Transform>(e).position = position;
    reg.Add<RigidBody>(e, RigidBody{type});
    reg.Add<Collider>(e, Collider{ColliderShape::Box, halfExtents});
    return e;
}

void Simulate(PhysicsWorld& physics, Scene& scene, float seconds) {
    for (int i = 0; i < int(seconds * 60.0f); ++i) {
        scene.UpdateWorldTransforms();
        physics.Step(scene, 1.0f / 60.0f);
    }
}

} // namespace

TEST_CASE("A dynamic box falls and comes to rest on a static floor") {
    Scene scene;
    AddBox(scene, "floor", {0, -0.5f, 0}, {10, 0.5f, 10}, BodyType::Static);
    Entity box = AddBox(scene, "box", {0, 3, 0}, glm::vec3(0.5f), BodyType::Dynamic);

    PhysicsWorld physics;
    physics.Start(scene);
    CHECK(physics.BodyCount() == 2);
    Simulate(physics, scene, 3.0f);

    float y = scene.GetRegistry().Get<Transform>(box).position.y;
    CHECK(y == doctest::Approx(0.5f).epsilon(0.05));
    physics.Stop(scene);
    CHECK_FALSE(scene.GetRegistry().Has<PhysicsBodyId>(box));
}

TEST_CASE("SDF shapes collide through generated meshes") {
    Scene scene;
    Entity ground = scene.CreateEntity("sdf ground");
    SdfShapeComponent slab;
    slab.primitive = Render::SdfPrimitive::Box;
    slab.size = {6.0f, 0.5f, 6.0f, 0.0f};
    scene.GetRegistry().Add<SdfShapeComponent>(ground, slab);
    scene.GetRegistry().Get<Transform>(ground).position = {0, -0.5f, 0};

    Entity ball = scene.CreateEntity("ball");
    scene.GetRegistry().Get<Transform>(ball).position = {0, 2, 0};
    scene.GetRegistry().Add<RigidBody>(ball);
    scene.GetRegistry().Add<Collider>(ball, Collider{ColliderShape::Sphere, glm::vec3(0.5f)});

    PhysicsWorld physics;
    physics.Start(scene);
    CHECK(physics.SdfCollisionTriangles() > 0);
    Simulate(physics, scene, 3.0f);
    CHECK(scene.GetRegistry().Get<Transform>(ball).position.y == doctest::Approx(0.5f).epsilon(0.1));
}

TEST_CASE("Raycasts report the entity that was hit") {
    Scene scene;
    Entity target = AddBox(scene, "target", {0, 0, -5}, glm::vec3(1.0f), BodyType::Static);
    PhysicsWorld physics;
    physics.Start(scene);
    auto hit = physics.Raycast({0, 0, 0}, {0, 0, -1}, 100.0f);
    REQUIRE(hit);
    CHECK(hit->entity == target);
    CHECK(hit->distance == doctest::Approx(4.0f).epsilon(0.01));
    CHECK(hit->normal.z == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("Bodies follow entities spawned and destroyed while the simulation runs") {
    Scene scene;
    AddBox(scene, "floor", {0, -0.5f, 0}, {10, 0.5f, 10}, BodyType::Static);
    PhysicsWorld physics;
    physics.Start(scene);
    CHECK(physics.BodyCount() == 1);

    Entity late = AddBox(scene, "late", {0, 2, 0}, glm::vec3(0.5f), BodyType::Dynamic);
    Simulate(physics, scene, 2.0f);
    CHECK(physics.BodyCount() == 2);
    CHECK(scene.GetRegistry().Get<Transform>(late).position.y == doctest::Approx(0.5f).epsilon(0.05));

    scene.DestroyEntity(late);
    Simulate(physics, scene, 0.1f);
    CHECK(physics.BodyCount() == 1);

    Entity other = AddBox(scene, "other", {3, 1, 0}, glm::vec3(0.5f), BodyType::Dynamic);
    Simulate(physics, scene, 0.1f);
    scene.GetRegistry().Remove<Collider>(other); // losing the collider removes the body
    Simulate(physics, scene, 0.1f);
    CHECK(physics.BodyCount() == 1);
    CHECK_FALSE(scene.GetRegistry().Has<PhysicsBodyId>(other));
}
