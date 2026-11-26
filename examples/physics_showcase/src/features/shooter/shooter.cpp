#include "features/shooter/shooter.hpp"

#include "features/lifetime/lifetime.hpp"

using namespace astral;

namespace shooter {

void install_shooter(FeatureContext& feature) {
    const MaterialHandle bullet_mat =
        feature.game.materials().create_material({.base_color = {0.95f, 0.3f, 0.2f, 1}, .roughness = 0.3f});
    feature.system(
        Phase::Update, "shooter::ShootSystem",
        [bullet_mat](Context& ctx) {
            Vec3 origin{};
            Vec3 forward{0, 0, -1};
            ctx.world.each<const Transform, const Camera>([&](const Transform& t, const Camera&) {
                origin = t.position;
                forward = t.rotation * Vec3{0, 0, -1};
            });
            if (ctx.actions.pressed("shoot")) {
                ctx.world.commands().spawn(
                    Transform{.position = origin + forward * 1.5f, .scale = {0.6f, 0.6f, 0.6f}},
                    MeshRenderer{ctx.meshes.sphere, bullet_mat},
                    RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f),
                                       .linear_velocity = forward * 30.0f,
                                       .mass = 20.0f,
                                       .continuous = true}},
                    lifetime::Lifetime{});
            }
            if (ctx.actions.pressed("push")) {
                if (const auto hit = ctx.physics_service().raycast(origin, forward, 200.0f)) {
                    if (const Entity target = entity_of(*hit); target.valid()) {
                        ctx.physics_service().add_impulse(target, forward * 400.0f);
                    }
                }
            }
        },
        SystemAccess{}.reads<Transform, Camera>());
}

} // namespace shooter
