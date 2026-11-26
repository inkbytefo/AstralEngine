// The joints station (north): a motorized hinge arm (Luau drives the motor), a SwingTwist limb in a cone, a SixDOF
// carriage, a weight on a joint that breaks under the load, a batched raycast sensor array, and a scene-authored part
// (joints_station.scene: a character that walks across a sensor, a crate on a motorized slider).
#include "stations/stations.hpp"

#include "features/lab/lab.hpp"

#include <vector>

using namespace astral;

namespace physics_showcase {

namespace {

void build_motors(Context& ctx, Handles& h) {
    World& w = ctx.world;
    const Vec3 c = joints_centre;
    const MaterialHandle arm_mat = ctx.materials().create_material({.base_color = {0.2f, 0.7f, 0.95f, 1}, .roughness = 0.35f, .metallic = 0.5f});
    const MaterialHandle limb_mat = ctx.materials().create_material({.base_color = {0.95f, 0.65f, 0.2f, 1}, .roughness = 0.4f});

    // 1) Motorized hinge arm; the target angle comes from Luau (lab::MotorOscillator).
    h.arm_body = w.create(Name{"motor_arm"}, Transform{.position = c + Vec3{-5.0f, 3.0f, 0.0f}, .scale = {1.6f, 0.2f, 0.2f}},
                          MeshRenderer{ctx.meshes.cube, arm_mat},
                          RigidBody{.desc = {.shape = ShapeDesc::box({0.8f, 0.1f, 0.1f}), .mass = 4.0f, .gravity_factor = 0.0f}});
    w.create(Name{"motor_hinge"},
             Joint{.type = JointType::Hinge, .b = h.arm_body, .anchor = c + Vec3{-5.8f, 3.0f, 0.0f}, .axis = {0, 1, 0},
                   .limited = true, .min_limit = -radians(85.0f), .max_limit = radians(85.0f)},
             JointMotor{.mode = JointMotorMode::Position, .frequency = 3.0f, .damping = 0.9f, .max_torque = 400.0f, .target_position = 0.5f},
             lab::MotorOscillator{.frequency = 0.5f, .amplitude = 1.0f});

    // 2) SwingTwist limb inside a 40-degree cone, twisted by a velocity motor.
    h.limb_body = w.create(Name{"swing_limb"}, Transform{.position = c + Vec3{-2.0f, 2.5f, 0.0f}, .scale = {0.3f, 1.0f, 0.3f}},
                           MeshRenderer{ctx.meshes.cube, limb_mat},
                           RigidBody{.desc = {.shape = ShapeDesc::capsule(0.15f, 0.35f), .mass = 3.0f}});
    w.create(Name{"swing_joint"},
             Joint{.type = JointType::SwingTwist, .b = h.limb_body, .anchor = c + Vec3{-2.0f, 3.2f, 0.0f}, .axis = {0, -1, 0},
                   .secondary_axis = {1, 0, 0}, .limited = true, .min_limit = -radians(25.0f), .max_limit = radians(25.0f),
                   .swing_limits = {radians(40.0f), radians(40.0f)}},
             JointMotor{.mode = JointMotorMode::Velocity, .max_torque = 120.0f, .target_angular_velocity = {0.0f, 2.0f, 0.0f}});

    // 3) SixDOF carriage: free along x within +-1 m, locked on y/z, limited rotation.
    h.slider_body = w.create(Name{"sixdof_carriage"}, Transform{.position = c + Vec3{1.0f, 2.5f, 0.0f}, .scale = {0.5f, 0.5f, 0.5f}},
                             MeshRenderer{ctx.meshes.cube, arm_mat},
                             RigidBody{.desc = {.shape = ShapeDesc::box({0.25f, 0.25f, 0.25f}), .mass = 2.0f, .gravity_factor = 0.0f}});
    w.create(Name{"sixdof_joint"},
             Joint{.type = JointType::SixDOF, .b = h.slider_body, .anchor = c + Vec3{1.0f, 2.5f, 0.0f}, .axis = {1, 0, 0},
                   .secondary_axis = {0, 1, 0}, .limited = true, .min_limit = -radians(30.0f), .max_limit = radians(30.0f),
                   .swing_limits = {radians(20.0f), radians(20.0f)},
                   .linear_limit_min = {-1.0f, 1.0f, 1.0f}, .linear_limit_max = {1.0f, -1.0f, -1.0f}});
}

void build_fragile_weight(Context& ctx, Handles& h) {
    World& w = ctx.world;
    // A 120 kg block (a cooked convex hull) on a fixed joint that breaks at 250 N.
    const ShapeDesc hull = ShapeDesc::convex_hull(
        {{-0.35f, -0.35f, -0.35f}, {0.35f, -0.35f, -0.35f}, {0.35f, -0.35f, 0.35f}, {-0.35f, -0.35f, 0.35f},
         {-0.35f, 0.35f, -0.35f},  {0.35f, 0.35f, -0.35f},  {0.35f, 0.35f, 0.35f},  {-0.35f, 0.35f, 0.35f}});
    const ShapeHandle shape = ctx.physics->create_shape_from_cooked(cook_physics_shape(hull));
    const Entity heavy = w.create(
        Name{"fragile_weight"}, Transform{.position = joints_centre + Vec3{5.5f, 5.0f, 0.0f}, .scale = {0.7f, 0.7f, 0.7f}},
        MeshRenderer{ctx.meshes.cube, ctx.materials().create_material({.base_color = {0.55f, 0.45f, 0.35f, 1}, .roughness = 0.7f})},
        RigidBody{.desc = {.mass = 120.0f, .cooked_shape = shape}});
    h.fragile_joint = w.create(Name{"fragile_joint"},
                               Joint{.type = JointType::Fixed, .b = heavy, .anchor = joints_centre + Vec3{5.5f, 5.5f, 0.0f}, .break_force = 250.0f});
}

} // namespace

void build_joints_station(Context& ctx, Handles& h) {
    build_motors(ctx, h);
    build_fragile_weight(ctx, h);
    ctx.world.create(Name{"sensor_array"}, Transform{.position = joints_centre}, lab::SensorArray{});

    const LoadResult scene = ctx.scenes().load_scene("joints_station.scene");
    for (const auto& error : scene.errors) ctx.log.warn("joints station scene: {}", error);

    add_station(ctx, joints_centre, "Eklemler",
                "Motorlu menteşe (Luau) · SwingTwist konisi · SixDOF kızak · 250 N'da kırılan bağ · sahneden karakter ve kızak", 16.0f);
}

} // namespace physics_showcase
