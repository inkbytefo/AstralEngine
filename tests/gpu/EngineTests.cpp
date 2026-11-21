#include <doctest/doctest.h>

#include "Astral/Engine.hpp"

using namespace Astral;

namespace {

struct Spinner {
    float speed = 1.0f;
};

class SpinSystem : public System {
public:
    int updates = 0;
    void Update(FrameContext& ctx) override {
        ++updates;
        ctx.scene.GetRegistry().Each<Transform, Spinner>([&](Entity, Transform& t, Spinner& s) {
            t.rotation = glm::angleAxis(s.speed * ctx.deltaTime, glm::vec3(0, 1, 0)) * t.rotation;
        });
    }
};

class TestGame : public Game {
public:
    Entity crate, spinner;
    SpinSystem* spin = nullptr;

    void OnStart(Engine& engine) override {
        Scene& scene = engine.GetScene();
        Entity camera = scene.CreateEntity("Camera");
        scene.GetRegistry().Add<CameraComponent>(camera);
        scene.GetRegistry().Get<Transform>(camera).position = {0, 2, 8};

        Entity sun = scene.CreateEntity("Sun");
        scene.GetRegistry().Add<LightComponent>(sun, LightComponent{Render::LightType::Directional, glm::vec3(1), 3.0f});
        scene.GetRegistry().Get<Transform>(sun).rotation = glm::angleAxis(glm::radians(-50.0f), glm::vec3(1, 0, 0));

        Entity floor = scene.CreateEntity("Floor");
        auto& reg = scene.GetRegistry();
        reg.Add<MeshRenderer>(floor, MeshRenderer{"builtin:cube"});
        reg.Get<Transform>(floor).scale = {20, 1, 20};
        reg.Get<Transform>(floor).position = {0, -0.5f, 0};
        reg.Add<RigidBody>(floor, RigidBody{BodyType::Static});
        reg.Add<Collider>(floor, Collider{ColliderShape::Box, glm::vec3(0.5f)});

        crate = scene.CreateEntity("Crate");
        reg.Add<MeshRenderer>(crate, MeshRenderer{"builtin:cube"});
        reg.Get<Transform>(crate).position = {0, 4, 0};
        reg.Add<RigidBody>(crate);
        reg.Add<Collider>(crate, Collider{ColliderShape::Box, glm::vec3(0.5f)});

        spinner = scene.CreateEntity("Rock");
        SdfShapeComponent rock;
        rock.primitive = Render::SdfPrimitive::Torus;
        rock.size = {0.8f, 0.25f, 0, 0};
        reg.Add<SdfShapeComponent>(spinner, rock);
        reg.Add<Spinner>(spinner);
        reg.Get<Transform>(spinner).position = {3, 1, 0};

        spin = &engine.AddSystem<SpinSystem>(Phase::Update);
    }
};

} // namespace

TEST_CASE("The engine runs a game headless: systems, physics, rendering") {
    EngineConfig config;
    config.headless = true;
    config.width = 320;
    config.height = 180;
    config.maxFrames = 120;
    Engine engine(config);
    TestGame game;
    CHECK(engine.Run(game) == 0);

    auto& reg = engine.GetScene().GetRegistry();
    CHECK(game.spin->updates == 120);
    CHECK(reg.Get<Transform>(game.crate).position.y == doctest::Approx(0.5f).epsilon(0.05)); // landed on the floor
    CHECK(glm::angle(reg.Get<Transform>(game.spinner).rotation) > 0.5f);
    auto stats = engine.GetRenderer().GetStats();
    CHECK(stats.meshInstances == 2);
    CHECK(stats.sdfShapes == 1);
    CHECK(stats.lights == 1);
}

TEST_CASE("Scenes saved by one engine load in another") {
    std::string json;
    {
        EngineConfig config;
        config.headless = true;
        config.maxFrames = 2;
        Engine engine(config);
        TestGame game;
        engine.Run(game);
        json = SerializeScene(engine.GetScene());
    }
    EngineConfig config;
    config.headless = true;
    Engine engine(config);
    auto scene = std::make_unique<Scene>();
    std::string error;
    REQUIRE(DeserializeScene(json, *scene, error));
    engine.SetScene(std::move(scene));
    CHECK(engine.GetScene().FindByName("Crate"));
    engine.Simulate(1.0f / 60.0f);
    engine.SyncRender();
    CHECK(engine.GetRenderer().GetStats().meshInstances == 2);
}
