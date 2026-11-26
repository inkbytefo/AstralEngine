// The pile station (south): a pyramid of crates to knock over, balls that rain down and expire, and the player.
#include "stations/stations.hpp"

#include "features/player/player.hpp"
#include "features/rain/rain.hpp"
#include "features/showcase/showcase.hpp"

using namespace astral;

namespace physics_showcase {

void add_station(Context& ctx, Vec3 centre, const char* title, const char* hint, float radius) {
    ctx.world.create(Name{std::string("station: ") + title}, Transform{.position = centre},
                     showcase::Station{.title = title, .hint = hint, .radius = radius});
}

void build_pile_station(Context& ctx) {
    World& w = ctx.world;
    const MaterialHandle crate_mat = ctx.materials().create_material({.base_color = {0.85f, 0.6f, 0.3f, 1}, .roughness = 0.7f});
    constexpr int base = 6;
    for (int row = 0; row < base; ++row) {
        for (int i = 0; i < base - row; ++i) {
            const float x = (static_cast<float>(i) - static_cast<float>(base - row - 1) * 0.5f) * 1.05f;
            const float y = 0.5f + static_cast<float>(row) * 1.0f;
            w.create(Transform{.position = pile_centre + Vec3{x, y, -4}}, MeshRenderer{ctx.meshes.cube, crate_mat},
                     RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .friction = 0.7f}});
        }
    }
    w.create(Name{"rain"}, Transform{.position = pile_centre + Vec3{0, 11, -6}}, rain::RainSpawner{});
    add_station(ctx, pile_centre, "Yığın",
                "Sol tık: top at · Sağ tık: baktığını it · Space: zıpla · F: uçan kamera", 16.0f);
}

void build_player(Context& ctx, Handles& h, View view) {
    World& w = ctx.world;
    const Vec3 start = view == View::Joints     ? joints_centre + Vec3{0, 0.2f, 4}
                       : view == View::Surfaces ? surfaces_centre + Vec3{0, 0.2f, 12}
                       : view == View::Water    ? Vec3{-31, plateau_height + 0.2f, 4}
                                                : pile_centre + Vec3{0, 0.2f, 8};
    const float yaw = view == View::Water ? radians(-90.0f) : 0.0f; // the water view faces +x, across the lake
    // The controller's position is at the feet, so the visible body is a child lifted to the capsule's centre.
    h.player = w.create(Name{"player"}, Transform{.position = start}, CharacterController{});
    w.create(Transform{.position = {0, 0.9f, 0}, .scale = {0.8f, 1.8f, 0.8f}}, Parent{h.player},
             MeshRenderer{ctx.meshes.sphere, ctx.materials().create_material({.base_color = {0.3f, 0.9f, 0.45f, 1}, .roughness = 0.4f})});
    h.camera = w.create(Name{"camera"}, Transform{}, Camera{.far_plane = 500.0f}, player::PlayerCamera{.yaw = yaw});
}

} // namespace physics_showcase
