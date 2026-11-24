#include "game.hpp"

#include "features/lab/lab.hpp"

#include <vector>

using namespace astral;

namespace physics_lab {

namespace {

void setup_materials_and_floor(Context& ctx, ShapeHandle& out_crate_shape) {
    PhysicsWorld& phys = *ctx.physics;
    phys.register_material(1, {.friction = 0.02f, .restitution = 0.0f, .friction_combine = PhysicsCombineMode::Min,
                               .restitution_combine = PhysicsCombineMode::Average});
    phys.register_material(2, {.friction = 0.85f, .restitution = 0.90f, .friction_combine = PhysicsCombineMode::Average,
                               .restitution_combine = PhysicsCombineMode::Max});
    phys.register_material(3, {.friction = 0.85f, .restitution = 0.02f, .friction_combine = PhysicsCombineMode::Average,
                               .restitution_combine = PhysicsCombineMode::Average});

    const ShapeDesc floor_mesh = ShapeDesc::mesh(
        {{-20, 0, -20}, {20, 0, -20}, {20, 0, 20}, {-20, 0, 20}}, {0, 2, 1, 0, 3, 2});
    const std::vector<std::byte> cooked_floor = cook_physics_shape(floor_mesh);
    const ShapeHandle floor_shape = phys.create_shape_from_cooked(cooked_floor);

    const ShapeDesc crate_hull = ShapeDesc::convex_hull(
        {{-0.35f, -0.35f, -0.35f}, {0.35f, -0.35f, -0.35f}, {0.35f, -0.35f, 0.35f}, {-0.35f, -0.35f, 0.35f},
         {-0.35f, 0.35f, -0.35f},  {0.35f, 0.35f, -0.35f},  {0.35f, 0.35f, 0.35f},  {-0.35f, 0.35f, 0.35f}});
    out_crate_shape = phys.create_shape_from_cooked(cook_physics_shape(crate_hull));

    ctx.world.create(Name{"lab_floor"}, Transform{.position = {0, 0, 0}, .scale = {40, 1, 40}},
                     MeshRenderer{ctx.meshes.plane, ctx.materials().create_material({.base_color = {0.22f, 0.24f, 0.28f, 1}, .roughness = 0.85f})},
                     RigidBody{.desc = {.motion = MotionType::Static, .surface = 3, .cooked_shape = floor_shape}});
}

void spawn_motor_station(Context& ctx, PhysicsLabGame& g) {
    World& w = ctx.world;
    const MaterialHandle arm_mat = ctx.materials().create_material({.base_color = {0.2f, 0.7f, 0.95f, 1}, .roughness = 0.35f, .metallic = 0.5f});
    const MaterialHandle limb_mat = ctx.materials().create_material({.base_color = {0.95f, 0.65f, 0.2f, 1}, .roughness = 0.4f});

    // 1) Motorized Hinge arm driven by Luau MotorOscillator.
    g.arm_body = w.create(Name{"motor_arm"}, Transform{.position = {-5.0f, 3.0f, 0.0f}, .scale = {1.6f, 0.2f, 0.2f}},
                          MeshRenderer{ctx.meshes.cube, arm_mat},
                          RigidBody{.desc = {.shape = ShapeDesc::box({0.8f, 0.1f, 0.1f}), .mass = 4.0f, .gravity_factor = 0.0f}});
    g.hinge_joint = w.create(
        Name{"motor_hinge"},
        Joint{.type = JointType::Hinge, .b = g.arm_body, .anchor = {-5.8f, 3.0f, 0.0f}, .axis = {0, 1, 0},
              .limited = true, .min_limit = -radians(85.0f), .max_limit = radians(85.0f)},
        JointMotor{.mode = JointMotorMode::Position, .frequency = 3.0f, .damping = 0.9f, .max_torque = 400.0f, .target_position = 0.5f},
        lab::MotorOscillator{.frequency = 0.5f, .amplitude = 1.0f});

    // 2) SwingTwist limb constrained inside a 40-degree cone.
    g.limb_body = w.create(Name{"swing_limb"}, Transform{.position = {-2.0f, 2.5f, 0.0f}, .scale = {0.3f, 1.0f, 0.3f}},
                           MeshRenderer{ctx.meshes.cube, limb_mat},
                           RigidBody{.desc = {.shape = ShapeDesc::capsule(0.15f, 0.35f), .mass = 3.0f}});
    g.swing_joint = w.create(
        Name{"swing_joint"},
        Joint{.type = JointType::SwingTwist, .b = g.limb_body, .anchor = {-2.0f, 3.2f, 0.0f}, .axis = {0, -1, 0},
              .secondary_axis = {1, 0, 0}, .limited = true, .min_limit = -radians(25.0f), .max_limit = radians(25.0f),
              .swing_limits = {radians(40.0f), radians(40.0f)}},
        JointMotor{.mode = JointMotorMode::Velocity, .max_torque = 120.0f, .target_angular_velocity = {0.0f, 2.0f, 0.0f}});

    // 3) SixDOF constrained slider carriage.
    g.slider_body = w.create(Name{"sixdof_carriage"}, Transform{.position = {1.0f, 2.5f, 0.0f}, .scale = {0.5f, 0.5f, 0.5f}},
                             MeshRenderer{ctx.meshes.cube, arm_mat},
                             RigidBody{.desc = {.shape = ShapeDesc::box({0.25f, 0.25f, 0.25f}), .mass = 2.0f, .gravity_factor = 0.0f}});
    g.sixdof_joint = w.create(
        Name{"sixdof_joint"},
        Joint{.type = JointType::SixDOF, .b = g.slider_body, .anchor = {1.0f, 2.5f, 0.0f}, .axis = {1, 0, 0},
              .secondary_axis = {0, 1, 0}, .limited = true, .min_limit = -radians(30.0f), .max_limit = radians(30.0f),
              .swing_limits = {radians(20.0f), radians(20.0f)},
              .linear_limit_min = {-1.0f, 1.0f, 1.0f}, .linear_limit_max = {1.0f, -1.0f, -1.0f}});
}

void spawn_breakable_and_materials(Context& ctx, PhysicsLabGame& g, ShapeHandle crate_shape) {
    World& w = ctx.world;
    const MaterialHandle ice_mat = ctx.materials().create_material({.base_color = {0.65f, 0.9f, 1.0f, 1}, .roughness = 0.1f});
    const MaterialHandle rubber_mat = ctx.materials().create_material({.base_color = {0.9f, 0.2f, 0.3f, 1}, .roughness = 0.5f});
    const MaterialHandle heavy_mat = ctx.materials().create_material({.base_color = {0.55f, 0.45f, 0.35f, 1}, .roughness = 0.7f});

    // Breakable overhead weight anchored with a fragile Fixed joint.
    const Entity heavy = w.create(
        Name{"fragile_weight"}, Transform{.position = {5.5f, 5.0f, 0.0f}, .scale = {0.7f, 0.7f, 0.7f}},
        MeshRenderer{ctx.meshes.cube, heavy_mat},
        RigidBody{.desc = {.mass = 120.0f, .surface = 3, .cooked_shape = crate_shape}});
    g.fragile_joint = w.create(
        Name{"fragile_joint"},
        Joint{.type = JointType::Fixed, .b = heavy, .anchor = {5.5f, 5.5f, 0.0f}, .break_force = 250.0f});

    // Material test pucks (ice vs grip) + bouncy rubber sphere.
    g.ice_puck = w.create(Name{"ice_puck"}, Transform{.position = {-6.0f, 0.25f, 6.0f}, .scale = {0.6f, 0.3f, 0.6f}},
                          MeshRenderer{ctx.meshes.cube, ice_mat},
                          RigidBody{.desc = {.shape = ShapeDesc::box({0.3f, 0.15f, 0.3f}), .linear_velocity = {6, 0, 0},
                                             .linear_damping = 0.0f, .surface = 1}});
    g.grip_puck = w.create(Name{"grip_puck"}, Transform{.position = {-6.0f, 0.25f, 8.0f}, .scale = {0.6f, 0.3f, 0.6f}},
                           MeshRenderer{ctx.meshes.cube, heavy_mat},
                           RigidBody{.desc = {.shape = ShapeDesc::box({0.3f, 0.15f, 0.3f}), .linear_velocity = {6, 0, 0},
                                              .linear_damping = 0.0f, .surface = 3}});
    g.rubber_ball = w.create(Name{"rubber_ball"}, Transform{.position = {0.0f, 3.5f, 7.0f}, .scale = {0.6f, 0.6f, 0.6f}},
                             MeshRenderer{ctx.meshes.sphere, rubber_mat},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f), .linear_damping = 0.0f, .surface = 2}});
}

} // namespace

void PhysicsLabGame::on_start(Context& ctx) {
    FeatureSet features;
    features.add("lab", lab::install_lab);
    features.install(ctx);

    const LoadResult scene = ctx.scenes().load_scene("component_demo.scene");
    for (const auto& error : scene.errors) ctx.log.warn("physics lab scene: {}", error);

    ctx.sun = {.direction = normalize(Vec3{-0.4f, -0.8f, -0.4f}), .color = {1.0f, 0.97f, 0.92f}, .intensity = 3.0f};
    ctx.environment.ambient = 0.4f;
    ctx.debug_show.physics = show_overlays_;
    ctx.debug_show.physics_joints = show_overlays_;

    ctx.world.create(Name{"camera"},
                     Transform{.position = {0.0f, 6.5f, 16.0f}, .rotation = Quat::from_axis_angle({1, 0, 0}, radians(-16.0f))},
                     Camera{.fov_y = radians(50.0f), .far_plane = 400.0f});

    ShapeHandle crate_shape{};
    setup_materials_and_floor(ctx, crate_shape);
    spawn_motor_station(ctx, *this);
    spawn_breakable_and_materials(ctx, *this, crate_shape);
}

} // namespace physics_lab
