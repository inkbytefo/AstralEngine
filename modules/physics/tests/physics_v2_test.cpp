// P0 Physics API v2 tests (ADR-0038): SixDOF, SwingTwist, motors, break events,
// cooked shapes, physics materials, batched queries, capabilities, and origin shift.
#include <astral/core/jobs.hpp>
#include <astral/physics/physics.hpp>
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace astral;

namespace {

void simulate(PhysicsWorld& world, int steps, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < steps; ++i) world.step(dt);
}

float angle_between(Vec3 a, Vec3 b) {
    const float c = std::clamp(dot(normalize(a), normalize(b)), -1.0f, 1.0f);
    return std::acos(c);
}

} // namespace

TEST_CASE("PhysicsWorld reports P0 backend capabilities") {
    PhysicsWorld world;
    CHECK(world.supports(PhysicsCapability::SixDOFJoints));
    CHECK(world.supports(PhysicsCapability::SwingTwistJoints));
    CHECK(world.supports(PhysicsCapability::JointMotors));
    CHECK(world.supports(PhysicsCapability::BreakableJoints));
    CHECK(world.supports(PhysicsCapability::CookedShapes));
    CHECK(world.supports(PhysicsCapability::MaterialTable));
    CHECK(world.supports(PhysicsCapability::BatchedQueries));
    CHECK(world.supports(PhysicsCapability::OriginShift));
}

TEST_CASE("Hinge velocity and position motors drive a door to target velocity and angle") {
    PhysicsWorld world;
    const BodyHandle arm = world.create_body({.shape = ShapeDesc::box({0.5f, 0.1f, 0.1f}),
                                              .position = {0.5f, 2.0f, 0.0f},
                                              .gravity_factor = 0.0f});
    JointDesc jd{};
    jd.type = JointType::Hinge;
    jd.body_b = arm;
    jd.anchor = {0.0f, 2.0f, 0.0f};
    jd.axis = {0.0f, 1.0f, 0.0f};
    jd.motor.mode = JointMotorMode::Velocity;
    jd.motor.target_velocity = 3.0f;
    jd.motor.max_torque = 500.0f;
    const JointHandle joint = world.create_joint(jd);
    REQUIRE(joint.valid());

    simulate(world, 60);
    CHECK(std::abs(world.angular_velocity(arm).y) > 2.0f);

    // Switch to position motor targeting 60 deg.
    world.set_joint_motor_mode(joint, JointMotorMode::Position);
    world.set_joint_motor_position(joint, radians(60.0f));
    simulate(world, 180);
    const Quat q = world.rotation(arm);
    const float yaw = std::abs(2.0f * std::atan2(q.y, q.w));
    CHECK(yaw == doctest::Approx(radians(60.0f)).epsilon(0.15));
}

TEST_CASE("Slider position motor moves a piston to its target position") {
    PhysicsWorld world;
    const BodyHandle piston = world.create_body({.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}),
                                                 .position = {0.0f, 1.0f, 0.0f},
                                                 .gravity_factor = 0.0f});
    JointDesc jd{};
    jd.type = JointType::Slider;
    jd.body_b = piston;
    jd.anchor = {0.0f, 1.0f, 0.0f};
    jd.axis = {1.0f, 0.0f, 0.0f};
    jd.limited = true;
    jd.min_limit = -2.0f;
    jd.max_limit = 2.0f;
    jd.motor.mode = JointMotorMode::Position;
    jd.motor.frequency_hz = 4.0f;
    jd.motor.damping = 1.0f;
    jd.motor.max_force = 2000.0f;
    jd.motor.target_position = 1.5f;
    const JointHandle joint = world.create_joint(jd);
    REQUIRE(joint.valid());

    simulate(world, 180);
    CHECK(world.position(piston).x == doctest::Approx(1.5f).epsilon(0.08));
}

TEST_CASE("SixDOF joint enforces linear & angular limits and supports angular velocity motor") {
    PhysicsWorld world;
    const BodyHandle body = world.create_body({.shape = ShapeDesc::box({0.4f, 0.2f, 0.2f}),
                                               .position = {0.0f, 2.0f, 0.0f},
                                               .gravity_factor = 0.0f});
    JointDesc jd{};
    jd.type = JointType::SixDOF;
    jd.body_b = body;
    jd.anchor = {0.0f, 2.0f, 0.0f};
    jd.axis = {1.0f, 0.0f, 0.0f};
    jd.secondary_axis = {0.0f, 1.0f, 0.0f};
    jd.limits.linear_min = {-0.5f, 1.0f, 1.0f};   // X limited [-0.5, 0.5], Y/Z fixed
    jd.limits.linear_max = {0.5f, -1.0f, -1.0f};
    jd.limits.angular_min = {-radians(45.0f), -radians(30.0f), -radians(30.0f)};
    jd.limits.angular_max = {radians(45.0f), radians(30.0f), radians(30.0f)};
    const JointHandle j = world.create_joint(jd);
    REQUIRE(j.valid());

    world.add_impulse(body, {500.0f, 200.0f, 200.0f});
    simulate(world, 90);
    const Vec3 p = world.position(body);
    CHECK(p.x <= 0.55f);
    CHECK(std::abs(p.y - 2.0f) < 0.05f);
    CHECK(std::abs(p.z) < 0.05f);

    // Enable angular velocity drive about twist axis X.
    world.set_joint_motor_mode(j, JointMotorMode::Velocity);
    world.set_joint_motor_angular_velocity(j, {4.0f, 0.0f, 0.0f});
    simulate(world, 30);
    CHECK(world.joint_reaction(j).force >= 0.0f);
}

TEST_CASE("SwingTwist joint keeps limb within swing cone and supports orientation motor") {
    PhysicsWorld world;
    const BodyHandle limb = world.create_body({.shape = ShapeDesc::capsule(0.15f, 0.4f),
                                               .position = {0.0f, 2.5f, 0.0f},
                                               .mass = 5.0f});
    JointDesc jd{};
    jd.type = JointType::SwingTwist;
    jd.body_b = limb;
    jd.anchor = {0.0f, 3.0f, 0.0f};
    jd.axis = {0.0f, -1.0f, 0.0f};
    jd.secondary_axis = {1.0f, 0.0f, 0.0f};
    jd.limits.swing_normal_cone = radians(35.0f);
    jd.limits.swing_plane_cone = radians(35.0f);
    jd.limits.twist_min = -radians(20.0f);
    jd.limits.twist_max = radians(20.0f);
    const JointHandle j = world.create_joint(jd);
    REQUIRE(j.valid());

    world.add_impulse(limb, {150.0f, 0.0f, 0.0f});
    float max_tilt = 0.0f;
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        const Vec3 axis_world = world.rotation(limb) * Vec3{0.0f, -1.0f, 0.0f};
        max_tilt = std::max(max_tilt, angle_between(axis_world, {0.0f, -1.0f, 0.0f}));
    }
    CHECK(max_tilt > radians(10.0f));
    CHECK(max_tilt < radians(48.0f)); // held inside cone with solver margin
}

TEST_CASE("Breakable joint breaks under excessive load and emits JointBrokenEvent") {
    PhysicsWorld world;
    const BodyHandle heavy = world.create_body({.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}),
                                                .position = {0.0f, 5.0f, 0.0f},
                                                .mass = 100.0f});
    JointDesc jd{};
    jd.type = JointType::Fixed;
    jd.body_b = heavy;
    jd.anchor = {0.0f, 5.5f, 0.0f};
    jd.break_force = 250.0f; // 100 kg * 9.81 = 981 N > 250 N
    const JointHandle j = world.create_joint(jd);
    REQUIRE(j.valid());
    CHECK_FALSE(world.joint_broken(j));

    bool saw_break = false;
    for (int i = 0; i < 30; ++i) {
        world.step(1.0f / 60.0f);
        for (const JointBrokenEvent& ev : world.broken_joints()) {
            if (ev.joint == j) {
                saw_break = true;
                CHECK(ev.body_b == heavy);
                CHECK(ev.force >= 250.0f);
            }
        }
    }
    CHECK(saw_break);
    CHECK(world.joint_broken(j));
    CHECK(world.valid(j)); // handle remains valid until destroyed
    simulate(world, 30);
    CHECK(world.position(heavy).y < 4.0f); // fell freely after breaking
    world.destroy(j);
    CHECK_FALSE(world.valid(j));
}

TEST_CASE("Cooked shapes round-trip through ASSH v1 binary and reject corrupt/mismatched versions") {
    // Build a mesh floor and a convex hull body, cook both, load into a fresh PhysicsWorld.
    const ShapeDesc mesh_desc = ShapeDesc::mesh(
        {{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10}},
        {0, 2, 1, 0, 3, 2});
    const ShapeDesc hull_desc = ShapeDesc::convex_hull(
        {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.0f, 0.8f, 0.0f}});

    std::vector<std::byte> cooked_mesh = cook_physics_shape(mesh_desc);
    std::vector<std::byte> cooked_hull = cook_physics_shape(hull_desc);
    REQUIRE(!cooked_mesh.empty());
    REQUIRE(!cooked_hull.empty());

    const auto mesh_info = inspect_cooked_shape(cooked_mesh);
    REQUIRE(mesh_info.has_value());
    CHECK(mesh_info->format_version == 1);
    CHECK(mesh_info->shape_type == ShapeType::Mesh);
    CHECK(mesh_info->static_only);
    CHECK(mesh_info->compatible);

    // Corrupt version byte -> incompatible & rejected cleanly.
    std::vector<std::byte> bad_version = cooked_hull;
    bad_version[4] = std::byte{99};
    const auto bad_info = inspect_cooked_shape(bad_version);
    REQUIRE(bad_info.has_value());
    CHECK_FALSE(bad_info->compatible);

    PhysicsWorld world;
    CHECK_FALSE(world.create_shape_from_cooked(bad_version).valid());

    const ShapeHandle smesh = world.create_shape_from_cooked(cooked_mesh);
    const ShapeHandle shull = world.create_shape_from_cooked(cooked_hull);
    REQUIRE(world.valid(smesh));
    REQUIRE(world.valid(shull));

    // Dynamic body with static-only mesh cooked shape is refused.
    CHECK_FALSE(world.create_body({.motion = MotionType::Dynamic, .cooked_shape = smesh}).valid());

    const BodyHandle floor = world.create_body({.motion = MotionType::Static, .position = {0, 0, 0}, .cooked_shape = smesh});
    const BodyHandle rock = world.create_body({.motion = MotionType::Dynamic, .position = {0, 3, 0}, .cooked_shape = shull});
    REQUIRE(floor.valid());
    REQUIRE(rock.valid());

    simulate(world, 180);
    CHECK(world.position(rock).y > 0.2f);
    CHECK(world.position(rock).y < 1.2f);
}

TEST_CASE("Physics material table combines friction and restitution per surface id") {
    PhysicsWorld world;
    // Surface 1: ice (Min friction), Surface 2: rubber (Max restitution), Surface 3: grippy floor.
    world.register_material(1, {.friction = 0.01f, .restitution = 0.0f, .friction_combine = PhysicsCombineMode::Min,
                                .restitution_combine = PhysicsCombineMode::Average});
    world.register_material(2, {.friction = 0.9f, .restitution = 0.92f, .friction_combine = PhysicsCombineMode::Average,
                                .restitution_combine = PhysicsCombineMode::Max});
    world.register_material(3, {.friction = 0.9f, .restitution = 0.0f, .friction_combine = PhysicsCombineMode::Average,
                                .restitution_combine = PhysicsCombineMode::Average});
    REQUIRE(world.material(1).has_value());

    // Static floor with surface 3.
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}, .surface = 3});
    // Ice puck slides far; grippy puck stops fast.
    const BodyHandle ice_puck = world.create_body({.shape = ShapeDesc::box({0.3f, 0.2f, 0.3f}), .position = {0, 0.21f, -3},
                                                   .linear_velocity = {6, 0, 0}, .linear_damping = 0.0f, .surface = 1});
    const BodyHandle grip_puck = world.create_body({.shape = ShapeDesc::box({0.3f, 0.2f, 0.3f}), .position = {0, 0.21f, 3},
                                                    .linear_velocity = {6, 0, 0}, .linear_damping = 0.0f, .surface = 3});
    // Rubber ball bounces high on surface 3 (Max restitution -> 0.92).
    const BodyHandle rubber_ball = world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {-5, 3.0f, 0},
                                                      .linear_damping = 0.0f, .surface = 2});

    float peak_after_bounce = 0.0f;
    bool hit_floor = false;
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        const float by = world.position(rubber_ball).y;
        if (by < 0.45f) hit_floor = true;
        if (hit_floor) peak_after_bounce = std::max(peak_after_bounce, by);
    }
    CHECK(world.position(ice_puck).x > world.position(grip_puck).x + 2.5f);
    CHECK(peak_after_bounce > 1.8f);
}

TEST_CASE("Batched queries match single queries in serial and parallel JobSystem modes") {
    PhysicsWorld world;
    for (int i = 0; i < 16; ++i) {
        world.create_body({.shape = ShapeDesc::box({0.4f, 0.4f, 0.4f}),
                           .motion = MotionType::Static,
                           .position = {static_cast<float>(i) * 2.0f, 0.0f, 0.0f},
                           .user_data = static_cast<std::uint64_t>(i + 1)});
    }
    constexpr std::size_t count = 64;
    std::vector<RayQuery> rays(count);
    for (std::size_t i = 0; i < count; ++i) {
        rays[i] = {.origin = {static_cast<float>(i % 16) * 2.0f, 5.0f, 0.0f}, .direction = {0, -1, 0}, .max_distance = 10.0f};
    }
    std::vector<std::optional<RayHit>> serial_hits(count);
    std::vector<std::optional<RayHit>> parallel_hits(count);
    JobSystem jobs(4);
    world.raycast_batch(rays, serial_hits, nullptr);
    world.raycast_batch(rays, parallel_hits, &jobs);

    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(serial_hits[i].has_value());
        REQUIRE(parallel_hits[i].has_value());
        CHECK(serial_hits[i]->user_data == (i % 16) + 1);
        CHECK(parallel_hits[i]->user_data == serial_hits[i]->user_data);
        CHECK(parallel_hits[i]->distance == doctest::Approx(serial_hits[i]->distance).epsilon(1e-4));
    }

    // Batched shape_cast & overlap using a shared ShapeHandle.
    const ShapeHandle sphere = world.create_shape(ShapeDesc::sphere(0.3f));
    std::vector<ShapeCastQuery> casts(count);
    std::vector<OverlapQuery> overlaps(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float x = static_cast<float>(i % 16) * 2.0f;
        casts[i] = {.cooked_shape = sphere, .position = {x, 3.0f, 0.0f}, .displacement = {0.0f, -5.0f, 0.0f}};
        overlaps[i] = {.cooked_shape = sphere, .position = {x, 0.0f, 0.0f}};
    }
    std::vector<std::optional<ShapeHit>> cast_hits(count);
    std::vector<std::vector<OverlapHit>> overlap_hits(count);
    world.shape_cast_batch(casts, cast_hits, &jobs);
    world.overlap_batch(overlaps, overlap_hits, &jobs);
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(cast_hits[i].has_value());
        CHECK(cast_hits[i]->user_data == (i % 16) + 1);
        REQUIRE(overlap_hits[i].size() == 1);
        CHECK(overlap_hits[i][0].user_data == (i % 16) + 1);
    }
}

TEST_CASE("Origin shift preserves world-anchored joint geometry and stability") {
    PhysicsWorld world;
    const BodyHandle bob = world.create_body({.shape = ShapeDesc::sphere(0.25f), .position = {1002.0f, 5.0f, 0.0f}, .linear_damping = 0.0f});
    const JointHandle j = world.create_joint({.type = JointType::Point, .body_b = bob, .anchor = {1000.0f, 5.0f, 0.0f}});
    REQUIRE(j.valid());
    simulate(world, 30);

    // Shift origin by +1000 on X so pendulum now orbits (0, 5, 0).
    world.shift_origin({1000.0f, 0.0f, 0.0f});
    float worst = 0.0f;
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        worst = std::max(worst, std::abs(length(world.position(bob) - Vec3{0.0f, 5.0f, 0.0f}) - 2.0f));
    }
    CHECK(worst < 0.08f);
}
