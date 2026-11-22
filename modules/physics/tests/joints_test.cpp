// Joints: fixed, point, hinge, slider, distance (M12.3).
#include <astral/physics/physics.hpp>
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

using namespace astral;

namespace {
void simulate(PhysicsWorld& world, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) world.step(1.0f / 60.0f);
}
float yaw_of(Quat q) { return 2.0f * std::atan2(q.y, q.w); }
} // namespace

TEST_CASE("a point joint to the world makes a pendulum: the distance to the anchor stays") {
    PhysicsWorld world;
    const BodyHandle bob = world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {2, 5, 0}, .linear_damping = 0.0f});
    const JointHandle joint = world.create_joint({.type = JointType::Point, .body_b = bob, .anchor = {0, 5, 0}});
    REQUIRE(joint.valid());
    float lowest = 100.0f, worst = 0.0f;
    for (int i = 0; i < 240; ++i) {
        world.step(1.0f / 60.0f);
        const Vec3 p = world.position(bob);
        lowest = std::min(lowest, p.y);
        worst = std::max(worst, std::abs(length(p - Vec3{0, 5, 0}) - 2.0f));
    }
    CHECK(worst < 0.05f);
    CHECK(lowest == doctest::Approx(3.0f).epsilon(0.05)); // swung to the bottom of the circle
}

TEST_CASE("a hinge turns about its axis only; limits stop it") {
    PhysicsWorld world;
    // A door: 1 m wide plank, hinge at its left edge (x = 0), axis up.
    const BodyHandle door = world.create_body({.shape = ShapeDesc::box({0.5f, 1.0f, 0.05f}), .position = {0.5f, 1, 0}, .gravity_factor = 0.0f});
    world.create_joint({.type = JointType::Hinge, .body_b = door, .anchor = {0, 1, 0}, .axis = {0, 1, 0}, .limited = true,
                        .min_limit = 0.0f, .max_limit = radians(90.0f)});
    world.add_impulse(door, {0, 0, -300.0f}); // push it open (towards -z swings it to +yaw)
    float most = -10.0f;
    for (int i = 0; i < 180; ++i) {
        world.step(1.0f / 60.0f);
        most = std::max(most, yaw_of(world.rotation(door)));
        const Vec3 p = world.position(door);
        CHECK(std::abs(p.y - 1.0f) < 0.02f);                         // never leaves the hinge's plane
        CHECK(std::abs(length(Vec3{p.x, 0, p.z}) - 0.5f) < 0.03f);    // stays 0.5 m from the hinge
    }
    CHECK(most > radians(60.0f));
    CHECK(most < radians(95.0f)); // the limit held
}

TEST_CASE("a slider moves along its axis only") {
    PhysicsWorld world;
    const BodyHandle cart = world.create_body({.shape = ShapeDesc::box({0.4f, 0.2f, 0.4f}), .position = {0, 1, 0}, .gravity_factor = 0.0f});
    world.create_joint({.type = JointType::Slider, .body_b = cart, .anchor = {0, 1, 0}, .axis = {1, 0, 0}, .limited = true, .min_limit = -1.0f, .max_limit = 2.0f});
    world.add_impulse(cart, {200.0f, 50.0f, 80.0f}); // pushed along x, up and sideways
    simulate(world, 2.0f);
    const Vec3 p = world.position(cart);
    CHECK(std::abs(p.y - 1.0f) < 0.02f);
    CHECK(std::abs(p.z) < 0.02f);
    CHECK(p.x > 1.0f);
    CHECK(p.x < 2.05f); // stopped by the limit
}

TEST_CASE("a fixed joint welds two bodies: they fall as one") {
    PhysicsWorld world;
    const BodyHandle a = world.create_body({.shape = ShapeDesc::box({0.4f, 0.4f, 0.4f}), .position = {0, 6, 0}});
    const BodyHandle b = world.create_body({.shape = ShapeDesc::box({0.4f, 0.4f, 0.4f}), .position = {1.0f, 6, 0}});
    world.create_joint({.type = JointType::Fixed, .body_a = a, .body_b = b, .anchor = {0.5f, 6, 0}});
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}});
    simulate(world, 3.0f);
    CHECK(length(world.position(b) - world.position(a)) == doctest::Approx(1.0f).epsilon(0.03));
    CHECK(world.position(a).y == doctest::Approx(0.4f).epsilon(0.2)); // landed together, both at rest
}

TEST_CASE("a distance joint with a spring bounces and settles") {
    PhysicsWorld world;
    const BodyHandle weight = world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {0, 4, 0}, .linear_damping = 0.0f});
    world.create_joint({.type = JointType::Distance, .body_b = weight, .anchor = {0, 6, 0}, .anchor_b = {0, 4, 0}, .limited = true,
                        .min_limit = 2.0f, .max_limit = 2.0f, .spring_frequency = 0.8f, .spring_damping = 0.3f});
    float lowest = 100.0f, highest = -100.0f;
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        lowest = std::min(lowest, world.position(weight).y);
        highest = std::max(highest, world.position(weight).y);
    }
    CHECK(lowest < 3.5f);   // the spring stretched under gravity (static stretch g / (2 pi f)^2 = 0.39 m, overshooting)
    CHECK(highest > 3.99f); // it started at its rest length
    simulate(world, 8.0f);
    CHECK(length(world.linear_velocity(weight)) < 0.2f); // damped
    CHECK(world.position(weight).y == doctest::Approx(4.0f - 0.39f).epsilon(0.05)); // hangs at the static stretch
}

TEST_CASE("joints disappear with their bodies; bad joints are refused") {
    PhysicsWorld world;
    const BodyHandle a = world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {0, 5, 0}});
    const BodyHandle b = world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {1, 5, 0}});
    const JointHandle joint = world.create_joint({.type = JointType::Point, .body_a = a, .body_b = b, .anchor = {0.5f, 5, 0}});
    REQUIRE(joint.valid());
    CHECK(world.valid(joint));
    world.destroy(a); // the joint goes with it, the other body is free
    CHECK_FALSE(world.valid(joint));
    simulate(world, 0.5f);
    CHECK(world.position(b).y < 4.9f); // falls on its own
    CHECK_FALSE(world.create_joint({.type = JointType::Point, .anchor = {0, 0, 0}}).valid());                      // no bodies at all
    CHECK_FALSE(world.create_joint({.type = JointType::Point, .body_a = b, .body_b = b, .anchor = {0, 0, 0}}).valid()); // one body twice
    CHECK_FALSE(world.create_joint({.type = JointType::Point, .body_b = a, .anchor = {0, 0, 0}}).valid());          // destroyed body
    CHECK_FALSE(world.create_joint({.type = JointType::Hinge, .body_b = b, .anchor = {0, 0, 0}, .axis = {0, 0, 0}}).valid()); // no axis
    const JointHandle ok = world.create_joint({.type = JointType::Point, .body_b = b, .anchor = {1, 5, 0}});
    REQUIRE(ok.valid());
    world.destroy(ok);
    CHECK_FALSE(world.valid(ok));
}

TEST_CASE("a plank bridge between two posts holds a crate") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({0.5f, 1.0f, 1.0f}), .motion = MotionType::Static, .position = {-0.5f, 4, 0}});
    world.create_body({.shape = ShapeDesc::box({0.5f, 1.0f, 1.0f}), .motion = MotionType::Static, .position = {10.5f, 4, 0}});
    constexpr int planks = 10;
    std::vector<BodyHandle> chain;
    for (int i = 0; i < planks; ++i) {
        chain.push_back(world.create_body({.shape = ShapeDesc::box({0.5f, 0.08f, 0.9f}), .position = {0.5f + static_cast<float>(i), 5, 0}, .friction = 1.0f}));
    }
    // Hinges between planks (axis z) and at both ends; the start is a point joint to the world post.
    world.create_joint({.type = JointType::Hinge, .body_b = chain.front(), .anchor = {0, 5, 0}, .axis = {0, 0, 1}});
    for (int i = 0; i + 1 < planks; ++i) {
        world.create_joint({.type = JointType::Hinge, .body_a = chain[static_cast<std::size_t>(i)], .body_b = chain[static_cast<std::size_t>(i + 1)],
                            .anchor = {static_cast<float>(i + 1), 5, 0}, .axis = {0, 0, 1}});
    }
    world.create_joint({.type = JointType::Hinge, .body_b = chain.back(), .anchor = {10, 5, 0}, .axis = {0, 0, 1}});
    world.create_body({.shape = ShapeDesc::box({0.4f, 0.4f, 0.4f}), .position = {5.0f, 7, 0}, .mass = 40.0f});
    simulate(world, 5.0f);
    float lowest = 100.0f;
    for (BodyHandle p : chain) lowest = std::min(lowest, world.position(p).y);
    CHECK(lowest > 2.5f);   // sagged under the crate but held
    CHECK(lowest < 4.99f);  // and it did sag
}
