#include <doctest/doctest.h>

#include "Astral/Engine/Scene/Scene.hpp"
#include "Astral/Engine/Scene/SceneSerializer.hpp"
#include "Astral/Engine/Scene/Serialization.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace Astral;

TEST_CASE("Child world transforms follow their parents") {
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child = scene.CreateEntity("child");
    scene.GetRegistry().Get<Transform>(parent).position = {10, 0, 0};
    scene.GetRegistry().Get<Transform>(child).position = {0, 2, 0};
    scene.SetParent(child, parent, false);

    scene.UpdateWorldTransforms();
    glm::vec3 world = glm::vec3(scene.GetRegistry().Get<WorldTransform>(child).matrix[3]);
    CHECK(world.x == doctest::Approx(10));
    CHECK(world.y == doctest::Approx(2));
    CHECK(scene.GetChildren(parent).size() == 1);
}

TEST_CASE("Re-parenting with keepWorldTransform does not move the child") {
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child = scene.CreateEntity("child");
    scene.GetRegistry().Get<Transform>(parent).position = {5, 0, 0};
    scene.GetRegistry().Get<Transform>(parent).scale = glm::vec3(2.0f);
    scene.GetRegistry().Get<Transform>(child).position = {1, 1, 1};

    scene.SetParent(child, parent, true);
    glm::vec3 world = glm::vec3(scene.GetWorldMatrix(child)[3]);
    CHECK(world.x == doctest::Approx(1));
    CHECK(world.y == doctest::Approx(1));
    CHECK(world.z == doctest::Approx(1));
}

TEST_CASE("Hierarchy refuses cycles and destroys children with their parent") {
    Scene scene;
    Entity a = scene.CreateEntity("a");
    Entity b = scene.CreateEntity("b");
    scene.SetParent(b, a);
    scene.SetParent(a, b); // would create a cycle: ignored
    CHECK_FALSE(scene.GetParent(a));

    scene.DestroyEntity(a);
    CHECK_FALSE(scene.IsValid(b));
}

TEST_CASE("Scenes round-trip through JSON") {
    Scene scene("Round trip");
    scene.environment.exposure = 1.5f;
    Entity root = scene.CreateEntity("root");
    Entity child = scene.CreateEntity("child");
    auto& reg = scene.GetRegistry();
    reg.Get<Transform>(child).position = {1, 2, 3};
    reg.Add<MeshRenderer>(child, MeshRenderer{"builtin:sphere", false, MaterialParams{glm::vec4(1, 0, 0, 1), 0.5f, 0.2f}, true});
    SdfShapeComponent sdf;
    sdf.primitive = Render::SdfPrimitive::Torus;
    sdf.operation = Render::SdfOperation::SmoothUnion;
    sdf.size = {1.0f, 0.25f, 0, 0};
    reg.Add<SdfShapeComponent>(root, sdf);
    reg.Add<LightComponent>(root, LightComponent{Render::LightType::Directional, glm::vec3(1), 3.0f});
    reg.Add<RigidBody>(child);
    reg.Add<Collider>(child, Collider{ColliderShape::Sphere, glm::vec3(0.5f)});
    scene.SetParent(child, root, false);

    std::string json = SerializeScene(scene);
    Scene loaded;
    std::string error;
    REQUIRE(DeserializeScene(json, loaded, error));
    CHECK(loaded.name == "Round trip");
    CHECK(loaded.environment.exposure == doctest::Approx(1.5f));

    Entity lchild = loaded.FindByName("child");
    Entity lroot = loaded.FindByName("root");
    REQUIRE(lchild);
    CHECK(loaded.GetParent(lchild) == lroot);
    auto& lreg = loaded.GetRegistry();
    CHECK(lreg.Get<Transform>(lchild).position.z == doctest::Approx(3));
    CHECK(lreg.Get<MeshRenderer>(lchild).mesh == "builtin:sphere");
    CHECK_FALSE(lreg.Get<MeshRenderer>(lchild).useAssetMaterials);
    CHECK(lreg.Get<SdfShapeComponent>(lroot).primitive == Render::SdfPrimitive::Torus);
    CHECK(lreg.Get<LightComponent>(lroot).type == Render::LightType::Directional);
    CHECK(lreg.Get<Collider>(lchild).shape == ColliderShape::Sphere);
    CHECK(lreg.Get<IdComponent>(lchild).id == reg.Get<IdComponent>(child).id);

    // Saving the loaded scene again gives identical text.
    CHECK(SerializeScene(loaded) == json);
}

TEST_CASE("Malformed scene files are rejected with a reason") {
    Scene scene;
    std::string error;
    CHECK_FALSE(DeserializeScene("{ not json", scene, error));
    CHECK(!error.empty());
    CHECK_FALSE(DeserializeScene(R"({"format":"something-else"})", scene, error));
    CHECK_FALSE(DeserializeScene(R"({"format":"astral-scene","version":999})", scene, error));
}

namespace {
struct Health { float value = 100.0f; };
} // namespace

TEST_CASE("Custom components can be registered for scene files") {
    RegisterComponent<Health>("Health", [](const Health& h) { return nlohmann::json{{"value", h.value}}; },
                              [](const nlohmann::json& j) { return Health{j.value("value", 100.0f)}; });
    Scene scene;
    Entity e = scene.CreateEntity("hero");
    scene.GetRegistry().Add<Health>(e, Health{42.0f});

    Scene loaded;
    std::string error;
    REQUIRE(DeserializeScene(SerializeScene(scene), loaded, error));
    CHECK(loaded.GetRegistry().Get<Health>(loaded.FindByName("hero")).value == doctest::Approx(42.0f));
}
