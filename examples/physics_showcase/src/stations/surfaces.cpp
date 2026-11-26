// The surfaces station (east): a physics material table (ice, rubber, grip) with combine modes, a cooked mesh floor,
// two pucks pushed at the same speed, a bouncing rubber ball, a wheel Luau spins with torque, and a scanner beam
// (raycast that ignores its own body and sees a sensor gate).
#include "stations/stations.hpp"

#include "features/showcase/showcase.hpp"

#include <vector>

using namespace astral;

namespace physics_showcase {

namespace {

constexpr std::uint16_t ice = 11, rubber = 12, grip = 13; // surface ids (1 and 2 are the terrain's and the lake toys')

void register_surfaces(PhysicsWorld& phys) {
    phys.register_material(ice, {.friction = 0.02f, .restitution = 0.0f, .friction_combine = PhysicsCombineMode::Min,
                                 .restitution_combine = PhysicsCombineMode::Average});
    phys.register_material(rubber, {.friction = 0.85f, .restitution = 0.90f, .friction_combine = PhysicsCombineMode::Average,
                                    .restitution_combine = PhysicsCombineMode::Max});
    phys.register_material(grip, {.friction = 0.85f, .restitution = 0.02f, .friction_combine = PhysicsCombineMode::Average,
                                  .restitution_combine = PhysicsCombineMode::Average});
}

void build_floor_and_pucks(Context& ctx, Handles& h) {
    World& w = ctx.world;
    const Vec3 c = surfaces_centre + Vec3{0, 0.02f, 0};
    // A cooked triangle-mesh floor (20 x 14 m) just above the plateau, with the grip material.
    const ShapeDesc floor_mesh = ShapeDesc::mesh({{-10, 0, -7}, {10, 0, -7}, {10, 0, 7}, {-10, 0, 7}}, {0, 2, 1, 0, 3, 2});
    const ShapeHandle floor_shape = ctx.physics->create_shape_from_cooked(cook_physics_shape(floor_mesh));
    w.create(Name{"surfaces_floor"}, Transform{.position = c, .scale = {20, 1, 14}},
             MeshRenderer{ctx.meshes.plane, ctx.materials().create_material({.base_color = {0.22f, 0.24f, 0.28f, 1}, .roughness = 0.85f})},
             RigidBody{.desc = {.motion = MotionType::Static, .surface = grip, .cooked_shape = floor_shape}});

    // Two pucks, same speed toward the lake: ice (min friction) slides much farther than grip.
    const MaterialHandle ice_mat = ctx.materials().create_material({.base_color = {0.65f, 0.9f, 1.0f, 1}, .roughness = 0.1f});
    const MaterialHandle grip_mat = ctx.materials().create_material({.base_color = {0.55f, 0.45f, 0.35f, 1}, .roughness = 0.7f});
    const auto puck = [&](const char* name, Vec3 at, MaterialHandle mat, std::uint16_t surface) {
        return w.create(Name{name}, Transform{.position = at, .scale = {0.6f, 0.3f, 0.6f}}, MeshRenderer{ctx.meshes.cube, mat},
                        RigidBody{.desc = {.shape = ShapeDesc::box({0.3f, 0.15f, 0.3f}), .linear_velocity = {-6, 0, 0},
                                           .linear_damping = 0.0f, .surface = surface}});
    };
    h.ice_puck = puck("ice_puck", c + Vec3{8, 0.25f, -2}, ice_mat, ice);
    h.grip_puck = puck("grip_puck", c + Vec3{8, 0.25f, 2}, grip_mat, grip);
    h.rubber_ball = w.create(Name{"rubber_ball"}, Transform{.position = c + Vec3{4, 3.5f, 5}, .scale = {0.6f, 0.6f, 0.6f}},
                             MeshRenderer{ctx.meshes.sphere, ctx.materials().create_material({.base_color = {0.9f, 0.2f, 0.3f, 1}, .roughness = 0.5f})},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f), .linear_damping = 0.0f, .surface = rubber}});
}

void build_wheel_and_scanner(Context& ctx, Handles& h) {
    World& w = ctx.world;
    const Vec3 c = surfaces_centre;
    const MaterialHandle metal = ctx.materials().create_material({.base_color = {0.8f, 0.82f, 0.85f, 1}, .roughness = 0.25f, .metallic = 1.0f});
    // A paddle wheel on a vertical hinge; Luau (showcase.luau) adds torque until it spins at 4 rad/s.
    h.wheel = w.create(Name{"wheel"}, Transform{.position = c + Vec3{-4, 1.0f, -4}, .scale = {3.0f, 0.2f, 0.4f}},
                       MeshRenderer{ctx.meshes.cube, metal},
                       RigidBody{.desc = {.shape = ShapeDesc::box({1.5f, 0.1f, 0.2f}), .mass = 6.0f, .gravity_factor = 0.0f}},
                       showcase::Spinner{});
    w.create(Name{"wheel_hinge"}, Joint{.type = JointType::Hinge, .b = h.wheel, .anchor = c + Vec3{-4, 1.0f, -4}, .axis = {0, 1, 0}});

    // A scanner post with its own collider; its beam must ignore that collider and stop at the sensor gate.
    h.scanner = w.create(Name{"scanner"}, Transform{.position = c + Vec3{-8, 1.0f, 4}, .scale = {0.4f, 0.4f, 0.4f}},
                         MeshRenderer{ctx.meshes.cube, metal},
                         RigidBody{.desc = {.shape = ShapeDesc::box({0.2f, 0.2f, 0.2f}), .motion = MotionType::Static}},
                         showcase::Scanner{.direction = {1, 0, 0}, .reach = 12.0f});
    h.scanner_gate = w.create(Name{"scanner_gate"}, Transform{.position = c + Vec3{-3, 1.0f, 4}},
                              RigidBody{.desc = {.shape = ShapeDesc::box({0.1f, 1.0f, 1.0f}), .motion = MotionType::Static, .sensor = true}});
}

} // namespace

void build_surfaces_station(Context& ctx, Handles& h) {
    register_surfaces(*ctx.physics);
    build_floor_and_pucks(ctx, h);
    build_wheel_and_scanner(ctx, h);
    add_station(ctx, surfaces_centre,
                "Yüzeyler",
                "Buz ve tutuş pakı aynı hızla · kauçuk top (Max geri sekme) · Luau torque ile dönen çark · ignore/sensors ile tarayıcı ışını", 16.0f);
}

} // namespace physics_showcase
