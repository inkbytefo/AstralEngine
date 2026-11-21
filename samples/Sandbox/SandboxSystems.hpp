#pragma once

// Gameplay code of the Sandbox sample: plain components plus systems that act on them.
// Nothing here touches the renderer or Jolt directly; it only uses the Engine API.

#include "Astral/Engine.hpp"

#include <algorithm>

namespace Sandbox {

using namespace Astral;

// ---- components ----

struct FlyCamera {
    float speed = 6.0f;
    float sensitivity = 0.0025f;
    float yaw = 0.0f;
    float pitch = -0.25f;
};

struct Spin {
    glm::vec3 axis{0, 1, 0};
    float speed = 0.6f; // radians per second
};

struct Lifetime {
    float seconds = 20.0f;
};

// ---- systems ----

// Right mouse button: look around. WASD + Q/E: move. Shift: faster.
class FlyCameraSystem final : public System {
public:
    void Update(FrameContext& ctx) override {
        Window* window = ctx.engine.GetWindow();
        const bool looking = ctx.input.IsDown(MouseButton::Right);
        if (window) window->SetCursorCaptured(looking);

        ctx.scene.GetRegistry().Each<FlyCamera, Transform>([&](Entity, FlyCamera& cam, Transform& t) {
            if (looking) {
                const glm::vec2 d = ctx.input.MouseDelta();
                cam.yaw -= d.x * cam.sensitivity;
                cam.pitch = std::clamp(cam.pitch - d.y * cam.sensitivity, -1.5f, 1.5f);
            }
            t.rotation = glm::angleAxis(cam.yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(cam.pitch, glm::vec3(1, 0, 0));

            const glm::vec3 move = t.Forward() * ctx.input.GetAxis("MoveForward") +
                                   t.Right() * ctx.input.GetAxis("MoveRight") +
                                   glm::vec3(0, 1, 0) * ctx.input.GetAxis("MoveUp");
            const float boost = ctx.input.IsActionDown("Boost") ? 3.0f : 1.0f;
            if (glm::dot(move, move) > 0.0f) t.position += glm::normalize(move) * cam.speed * boost * ctx.deltaTime;
        });
    }
};

class SpinSystem final : public System {
public:
    void Update(FrameContext& ctx) override {
        ctx.scene.GetRegistry().Each<Spin, Transform>([&](Entity, Spin& s, Transform& t) {
            t.rotation = glm::normalize(glm::angleAxis(s.speed * ctx.deltaTime, s.axis) * t.rotation);
        });
    }
};

// Left mouse button: throw a ball from the camera.
class ThrowSystem final : public System {
public:
    void Update(FrameContext& ctx) override {
        if (!ctx.input.WasActionPressed("Throw")) return;
        Registry& reg = ctx.scene.GetRegistry();
        Entity camera = ctx.scene.GetPrimaryCamera();
        if (!camera) return;
        const Transform& from = reg.Get<Transform>(camera);

        Entity ball = ctx.scene.CreateEntity("Ball");
        Transform& t = reg.Get<Transform>(ball);
        t.position = from.position + from.Forward() * 1.0f;
        t.scale = glm::vec3(0.35f);
        MeshRenderer mesh;
        mesh.mesh = "builtin:sphere";
        mesh.useAssetMaterials = false;
        mesh.material.baseColor = glm::vec4(0.9f, 0.35f + 0.1f * float(m_Thrown % 5), 0.15f, 1.0f);
        mesh.material.roughness = 0.35f;
        reg.Add<MeshRenderer>(ball, mesh);
        RigidBody body;
        body.mass = 2.0f;
        body.initialVelocity = from.Forward() * 18.0f;
        reg.Add<RigidBody>(ball, body);
        reg.Add<Collider>(ball, Collider{ColliderShape::Sphere, glm::vec3(0.5f)});
        reg.Add<Lifetime>(ball);
        ++m_Thrown;
    }

private:
    uint32_t m_Thrown = 0;
};

class LifetimeSystem final : public System {
public:
    void Update(FrameContext& ctx) override {
        std::vector<Entity> expired;
        ctx.scene.GetRegistry().Each<Lifetime>([&](Entity e, Lifetime& l) {
            l.seconds -= ctx.deltaTime;
            if (l.seconds <= 0.0f) expired.push_back(e);
        });
        for (Entity e : expired) ctx.scene.DestroyEntity(e);
    }
};

} // namespace Sandbox
