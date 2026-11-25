#include "features/playground/playground.hpp"

using namespace astral;

namespace playground {

void install_playground(FeatureContext& feature) {
    feature.component<JumpPad>();
    feature.component<Dropper>();
    feature.component<ContactStats>();
    // Spawns crates that float (Buoyant), drift in the wind (AirDrag), and are launched by the pad.
    const MaterialHandle crate_mat =
        feature.game.materials().create_material({.base_color = {0.85f, 0.6f, 0.3f, 1}, .roughness = 0.6f});
    feature.system(
        Phase::Update, "playground::DropperSystem",
        [crate_mat](Context& ctx) {
            ctx.world.each<Dropper, const Transform>([&](Dropper& d, const Transform& t) {
                d.timer += ctx.time.delta;
                if (d.timer < d.interval) return;
                d.timer = 0;
                ctx.world.commands().spawn(
                    Transform{.position = t.position, .scale = {0.6f, 0.6f, 0.6f}},
                    MeshRenderer{ctx.meshes.cube, crate_mat},
                    RigidBody{.desc = {.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}),
                                       .mass = 90.0f,
                                       .friction = 0.7f,
                                       .surface = 2}},
                    Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f},
                    AirDrag{.area = 0.36f, .drag = 1.0f});
            });
        },
        SystemAccess{}.query<Dropper, const Transform>());
}

} // namespace playground
