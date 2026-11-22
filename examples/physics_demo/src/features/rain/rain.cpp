#include "features/rain/rain.hpp"

#include "features/lifetime/lifetime.hpp"

#include <cmath>

using namespace astral;

namespace rain {

namespace {

class RainSystem final : public System {
public:
    void update(Context& ctx) override {
        ctx.world.each<RainSpawner>([&](RainSpawner& spawner) {
            spawner.timer += ctx.time.delta;
            if (spawner.timer <= spawner.interval) return;
            spawner.timer = 0;
            const float a = static_cast<float>(ctx.time.frame) * 0.37f;
            ctx.world.commands().spawn(Transform{.position = {std::cos(a) * 3.0f, 14.0f, std::sin(a) * 3.0f - 2.0f}},
                                       MeshRenderer{ctx.meshes.sphere, ctx.create_material({.base_color = {0.3f, 0.6f, 0.95f, 1}, .roughness = 0.25f, .metallic = 0.0f})},
                                       RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .restitution = 0.4f}}, lifetime::Lifetime{});
        });
    }
};

} // namespace

void install_rain(FeatureContext& feature) {
    feature.component<RainSpawner>();
    feature.system<RainSystem>(Phase::Update);
}

} // namespace rain
