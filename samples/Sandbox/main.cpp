// Sandbox: a small physics playground built only on the public Engine API.
//
//   Right mouse: look      WASD / Q E: move      Shift: faster
//   Left mouse: throw a ball      F5: save the scene to sandbox.scene.json
//   F2: cycle debug views (albedo, normals, roughness, ...)
//
//   Sandbox --headless [frames] [--screenshot out.ppm] [--debug-view N]

#include "SandboxSystems.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

using namespace Astral;
using namespace Sandbox;

namespace {

Entity AddMesh(Scene& scene, const char* name, const char* mesh, glm::vec3 position, glm::vec3 scale, glm::vec4 color,
               float roughness = 0.6f, float metallic = 0.0f) {
    Entity e = scene.CreateEntity(name);
    Registry& reg = scene.GetRegistry();
    reg.Get<Transform>(e).position = position;
    reg.Get<Transform>(e).scale = scale;
    MeshRenderer r;
    r.mesh = mesh;
    r.useAssetMaterials = false;
    r.material.baseColor = color;
    r.material.roughness = roughness;
    r.material.metallic = metallic;
    reg.Add<MeshRenderer>(e, r);
    return e;
}

Entity AddSdf(Scene& scene, const char* name, Render::SdfPrimitive primitive, Render::SdfOperation op, glm::vec3 position,
              glm::vec4 size, glm::vec3 albedo, float blend = 0.0f, int order = 0) {
    Entity e = scene.CreateEntity(name);
    SdfShapeComponent s;
    s.primitive = primitive;
    s.operation = op;
    s.size = size;
    s.blend = blend;
    s.order = order;
    s.material.albedo = albedo;
    s.material.roughness = 0.8f;
    scene.GetRegistry().Add<SdfShapeComponent>(e, s);
    scene.GetRegistry().Get<Transform>(e).position = position;
    return e;
}

void BuildScene(Scene& scene) {
    using Render::SdfOperation;
    using Render::SdfPrimitive;
    Registry& reg = scene.GetRegistry();
    scene.name = "Sandbox";

    Entity camera = scene.CreateEntity("Camera");
    reg.Add<CameraComponent>(camera);
    reg.Add<FlyCamera>(camera);
    reg.Get<Transform>(camera).position = {0.0f, 3.0f, 12.0f};

    Entity sun = scene.CreateEntity("Sun");
    reg.Add<LightComponent>(sun, LightComponent{Render::LightType::Directional, glm::vec3(1.0f, 0.95f, 0.88f), 4.0f});
    reg.Get<Transform>(sun).rotation =
        glm::angleAxis(glm::radians(-35.0f), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::radians(-40.0f), glm::vec3(1, 0, 0));

    // Ground: a static box mesh.
    Entity ground = AddMesh(scene, "Ground", "builtin:cube", {0, -0.5f, 0}, {40, 1, 40}, {0.45f, 0.45f, 0.42f, 1});
    reg.Add<RigidBody>(ground, RigidBody{BodyType::Static});
    reg.Add<Collider>(ground, Collider{ColliderShape::Box, glm::vec3(0.5f)});

    // A stack of crates to knock over.
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4 - y; ++x) {
            Entity crate = AddMesh(scene, "Crate", "builtin:cube", {-3.0f + x * 1.02f + y * 0.51f, 0.5f + y * 1.0f, -2.0f},
                                   glm::vec3(1.0f), {0.62f, 0.42f, 0.25f, 1}, 0.7f);
            reg.Add<RigidBody>(crate);
            reg.Add<Collider>(crate, Collider{ColliderShape::Box, glm::vec3(0.5f)});
        }

    // A shiny sphere on a pedestal.
    AddMesh(scene, "Chrome ball", "builtin:sphere", {4.0f, 1.5f, 0.0f}, glm::vec3(1.0f), {0.95f, 0.95f, 0.95f, 1}, 0.12f, 1.0f);
    Entity pedestal = AddMesh(scene, "Pedestal", "builtin:cube", {4.0f, 0.5f, 0.0f}, {0.8f, 1.0f, 0.8f}, {0.2f, 0.22f, 0.25f, 1});
    reg.Add<RigidBody>(pedestal, RigidBody{BodyType::Static});
    reg.Add<Collider>(pedestal, Collider{ColliderShape::Box, glm::vec3(0.5f)});

    // SDF terrain features with collision: a smooth hill, a carved rock and a spinning arch.
    AddSdf(scene, "Hill", SdfPrimitive::Sphere, SdfOperation::Union, {-6.0f, -2.2f, 4.0f}, glm::vec4(4.0f), {0.32f, 0.42f, 0.22f});
    AddSdf(scene, "Hill bump", SdfPrimitive::Sphere, SdfOperation::SmoothUnion, {-3.5f, -0.8f, 5.0f}, glm::vec4(1.8f),
           {0.32f, 0.42f, 0.22f}, 1.2f, 1);
    AddSdf(scene, "Rock", SdfPrimitive::Box, SdfOperation::Union, {6.0f, 1.0f, -5.0f}, {1.5f, 1.0f, 1.5f, 0.25f},
           {0.5f, 0.48f, 0.45f}, 0.0f, 2);
    AddSdf(scene, "Rock cut", SdfPrimitive::Sphere, SdfOperation::SmoothSubtract, {6.0f, 2.0f, -3.6f}, glm::vec4(1.2f),
           {0.5f, 0.48f, 0.45f}, 0.3f, 3);
    Entity arch = AddSdf(scene, "Arch", SdfPrimitive::Torus, SdfOperation::Union, {0.0f, 0.2f, -8.0f}, {2.5f, 0.45f, 0, 0},
                         {0.75f, 0.3f, 0.2f}, 0.0f, 4);
    reg.Get<Transform>(arch).rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0));
    reg.Add<Spin>(arch, Spin{glm::vec3(0, 0, 1), 0.4f});

    // A couple of warm point lights.
    for (glm::vec3 p : {glm::vec3(-2.0f, 2.5f, 1.0f), glm::vec3(3.0f, 2.0f, -6.0f)}) {
        Entity light = scene.CreateEntity("Lamp");
        reg.Add<LightComponent>(light, LightComponent{Render::LightType::Point, glm::vec3(1.0f, 0.7f, 0.4f), 6.0f, 8.0f});
        reg.Get<Transform>(light).position = p;
    }
}

void WritePpm(const char* path, const std::vector<uint8_t>& rgba, glm::uvec2 size) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P6 %u %u 255\n", size.x, size.y);
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) std::fwrite(&rgba[i], 1, 3, f);
    std::fclose(f);
}

class SandboxGame final : public Game {
public:
    const char* screenshot = nullptr;
    int debugView = 0;

    void OnStop(Engine& engine) override {
        if (!screenshot) return;
        Render::Renderer& r = engine.GetRenderer();
        WritePpm(screenshot, r.ReadViewPixels(engine.GetMainView()), r.GetViewSize(engine.GetMainView()));
    }

    void OnStart(Engine& engine) override {
        engine.GetViewSettings().debugView = Render::DebugView(debugView);
        Input& input = engine.GetInput();
        input.BindAxis("MoveForward", Key::W, Key::S);
        input.BindAxis("MoveRight", Key::D, Key::A);
        input.BindAxis("MoveUp", Key::E, Key::Q);
        input.BindAction("Boost", Key::LeftShift);
        input.BindAction("Throw", MouseButton::Left);
        input.BindAction("Save", Key::F5);
        input.BindAction("DebugView", Key::F2);
        input.BindAction("Quit", Key::Escape);

        BuildScene(engine.GetScene());
        engine.AddSystem<FlyCameraSystem>(Phase::Update);
        engine.AddSystem<ThrowSystem>(Phase::Update);
        engine.AddSystem<SpinSystem>(Phase::Update);
        engine.AddSystem<LifetimeSystem>(Phase::Update);
        engine.AddSystem<HotkeySystem>(Phase::LateUpdate);
    }

private:
    class HotkeySystem final : public System {
    public:
        void Update(FrameContext& ctx) override {
            if (ctx.input.WasActionPressed("Quit")) ctx.engine.Quit();
            if (ctx.input.WasActionPressed("DebugView")) {
                auto& view = ctx.engine.GetViewSettings().debugView;
                view = Render::DebugView((uint32_t(view) + 1) % (uint32_t(Render::DebugView::SdfSteps) + 1));
            }
            if (ctx.input.WasActionPressed("Save")) {
                std::string error;
                if (SaveScene(ctx.scene, "sandbox.scene.json", error)) Log::Info("sandbox", "Saved sandbox.scene.json");
                else Log::Error("sandbox", "Save failed: {}", error);
            }
        }
    };
};

} // namespace

int main(int argc, char** argv) {
    EngineConfig config;
    config.title = "Astral Sandbox";
    SandboxGame game;
    for (int i = 1; i < argc; ++i) {
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(argv[i], "--headless") == 0) {
            config.headless = true;
            config.maxFrames = (hasValue && argv[i + 1][0] != '-') ? std::atoll(argv[++i]) : 300;
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && hasValue) {
            game.screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--debug-view") == 0 && hasValue) {
            game.debugView = std::atoi(argv[++i]);
        }
    }
    return RunGame(game, config);
}
