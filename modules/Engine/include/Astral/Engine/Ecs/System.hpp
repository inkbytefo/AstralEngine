#pragma once

#include <cstdint>

namespace Astral {

class Engine;
class Scene;
class Input;

// When a system runs within a frame:
//   FixedUpdate - fixed timestep (default 60 Hz), before physics; gameplay that moves bodies
//   Update      - once per frame, variable timestep; most gameplay
//   LateUpdate  - after Update and transforms; cameras, follow logic
enum class Phase : uint32_t { FixedUpdate = 0, Update = 1, LateUpdate = 2 };

struct FrameContext {
    Engine& engine;
    Scene& scene;
    const Input& input;
    float deltaTime;     // seconds; the fixed step in FixedUpdate
    double time;         // seconds since play started
    uint64_t frame;
};

// Game logic lives in systems: stateless or small-state classes that query the ECS.
//
//     class Spin : public Astral::System {
//         void Update(Astral::FrameContext& ctx) override {
//             ctx.scene.GetRegistry().Each<Transform, Spinner>([&](Entity, Transform& t, Spinner& s) { ... });
//         }
//     };
//     engine.AddSystem<Spin>(Astral::Phase::Update);
class System {
public:
    virtual ~System() = default;
    virtual void OnStart(Engine&, Scene&) {}  // play started (scene loaded)
    virtual void Update(FrameContext& context) = 0;
    virtual void OnStop(Engine&, Scene&) {}   // play stopped
};

} // namespace Astral
