#include "features/shooter/shooter.hpp"

#include "features/lifetime/lifetime.hpp"

using namespace astral;

namespace shooter {

namespace {

class ShootSystem final : public System {
public:
    void update(Context& ctx) override {
        Vec3 origin{}, forward{0, 0, -1};
        ctx.world.each<const Transform, const Camera>([&](const Transform& t, const Camera&) {
            origin = t.position;
            forward = t.rotation * Vec3{0, 0, -1};
        });
        if (ctx.actions.pressed("shoot")) {
            ctx.world.create(Transform{.position = origin + forward * 1.5f, .scale = {0.6f, 0.6f, 0.6f}},
                             MeshRenderer{ctx.meshes.sphere, ctx.create_material({.base_color = {0.95f, 0.3f, 0.2f, 1}, .roughness = 0.3f})},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f), .linear_velocity = forward * 30.0f,
                                                .mass = 20.0f, .continuous = true}},
                             lifetime::Lifetime{});
        }
        if (ctx.actions.pressed("push")) {
            if (const auto hit = ctx.physics->raycast(origin, forward, 200.0f)) {
                if (const RigidBody* rb = ctx.world.get<RigidBody>(entity_of(*hit))) {
                    ctx.physics->add_impulse(rb->handle, forward * 400.0f);
                }
            }
        }
    }
};

} // namespace

void install_shooter(FeatureContext& feature) { feature.system<ShootSystem>(Phase::Update); }

} // namespace shooter
