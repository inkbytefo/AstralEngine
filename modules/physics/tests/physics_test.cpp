#include <astral/physics/physics.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <algorithm>
#include <limits>

using namespace astral;

namespace {
/// A static ground box whose top face is at y = 0.
BodyHandle make_ground(PhysicsWorld& world) {
    return world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static,
                              .position = {0, -0.5f, 0}});
}
void simulate(PhysicsWorld& world, float seconds) {
    const int steps = static_cast<int>(seconds * 60.0f);
    for (int i = 0; i < steps; ++i) world.step(1.0f / 60.0f);
}
} // namespace

TEST_CASE("a dynamic body falls under gravity") {
    PhysicsWorld world;
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 10, 0},
                                         .linear_damping = 0.0f});
    REQUIRE(ball.valid());
    simulate(world, 1.0f);
    CHECK(world.position(ball).y == doctest::Approx(10.0f - 4.905f).epsilon(0.03));
}

TEST_CASE("a sphere comes to rest on the ground") {
    PhysicsWorld world;
    make_ground(world);
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 3, 0}});
    simulate(world, 4.0f);
    CHECK(world.position(ball).y == doctest::Approx(0.5f).epsilon(0.05));
    CHECK(length(world.linear_velocity(ball)) < 0.05f);
}

TEST_CASE("static bodies do not move") {
    PhysicsWorld world;
    BodyHandle ground = make_ground(world);
    simulate(world, 1.0f);
    CHECK(world.position(ground).y == doctest::Approx(-0.5f));
}

TEST_CASE("raycast reports distance, normal and user data") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static,
                       .position = {0, -0.5f, 0}, .user_data = 42});
    const auto hit = world.raycast({0, 10, 0}, {0, -1, 0}, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(10.0f).epsilon(0.01));
    CHECK(hit->normal.y == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(hit->user_data == 42);
    CHECK_FALSE(world.raycast({0, 10, 0}, {0, 1, 0}, 100.0f).has_value());
    CHECK_FALSE(world.raycast({0, 10, 0}, {0, -1, 0}, 5.0f).has_value()); // too short
}

TEST_CASE("raycast can ignore a body") {
    PhysicsWorld world;
    make_ground(world);
    BodyHandle box = world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 3, 0}});
    const auto hit = world.raycast({0, 10, 0}, {0, -1, 0}, 100.0f, QueryFilter{.ignore = box});
    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(10.0f).epsilon(0.01));
}

TEST_CASE("destroyed bodies disappear from queries and their handles die") {
    PhysicsWorld world;
    BodyHandle ground = make_ground(world);
    CHECK(world.body_count() == 1);
    world.destroy(ground);
    CHECK_FALSE(world.valid(ground));
    CHECK(world.body_count() == 0);
    CHECK_FALSE(world.raycast({0, 10, 0}, {0, -1, 0}, 100.0f).has_value());
    CHECK(world.position(ground) == Vec3{}); // invalid handle: safe default
}

TEST_CASE("impulses and velocities change motion") {
    PhysicsWorld world({.gravity = {0, 0, 0}});
    BodyHandle box = world.create_body({.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .linear_damping = 0.0f});
    world.set_linear_velocity(box, {2, 0, 0});
    simulate(world, 1.0f);
    CHECK(world.position(box).x == doctest::Approx(2.0f).epsilon(0.02));
    world.add_impulse(box, Vec3{0, 0, 1} * world.mass(box) * 3.0f); // +3 m/s on z
    CHECK(world.linear_velocity(box).z == doctest::Approx(3.0f).epsilon(0.02));
}

TEST_CASE("kinematic bodies follow their target and push dynamic ones") {
    PhysicsWorld world({.gravity = {0, 0, 0}});
    BodyHandle pusher = world.create_body({.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .motion = MotionType::Kinematic});
    BodyHandle crate = world.create_body({.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .position = {2, 0, 0}});
    for (int i = 0; i < 60; ++i) {
        world.move_kinematic(pusher, {static_cast<float>(i + 1) * 0.05f, 0, 0}, {}, 1.0f / 60.0f);
        world.step(1.0f / 60.0f);
    }
    CHECK(world.position(pusher).x == doctest::Approx(3.0f).epsilon(0.02));
    CHECK(world.position(crate).x > 3.4f); // got pushed ahead
}

TEST_CASE("contacts are reported when bodies touch") {
    PhysicsWorld world;
    BodyHandle ground = make_ground(world);
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 2, 0}});
    bool touched = false;
    for (int i = 0; i < 120 && !touched; ++i) {
        world.step(1.0f / 60.0f);
        for (const ContactEvent& c : world.contacts()) {
            const bool pair = (c.a == ball && c.b == ground) || (c.a == ground && c.b == ball);
            touched |= pair && c.type == ContactEvent::Type::Begin;
        }
    }
    CHECK(touched);
}

TEST_CASE("sensors report overlaps without blocking") {
    PhysicsWorld world;
    BodyHandle sensor = world.create_body({.shape = ShapeDesc::box({2, 2, 2}), .motion = MotionType::Static,
                                           .position = {0, 0, 0}, .sensor = true});
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 5, 0}});
    bool entered = false;
    for (int i = 0; i < 90; ++i) {
        world.step(1.0f / 60.0f);
        for (const ContactEvent& c : world.contacts()) entered |= (c.a == sensor || c.b == sensor);
    }
    CHECK(entered);
    CHECK(world.position(ball).y < -2.0f); // fell straight through
}

TEST_CASE("a character walks on the ground and is blocked by walls") {
    PhysicsWorld world;
    make_ground(world);
    world.create_body({.shape = ShapeDesc::box({0.5f, 2, 5}), .motion = MotionType::Static, .position = {3, 2, 0}});
    CharacterHandle hero = world.create_character({.radius = 0.4f, .height = 1.8f, .position = {0, 1, 0}});
    REQUIRE(hero.valid());
    for (int i = 0; i < 60; ++i) world.update_character(hero, {0, 0, 0}, false, 1.0f / 60.0f); // settle
    CHECK(world.character_state(hero).on_ground);
    for (int i = 0; i < 180; ++i) world.update_character(hero, {4, 0, 0}, false, 1.0f / 60.0f);
    const Vec3 p = world.character_state(hero).position;
    CHECK(p.x < 2.6f);  // stopped by the wall at x = 2.5 (minus radius)
    CHECK(p.x > 1.5f);  // but did walk
    CHECK(p.y == doctest::Approx(0.0f).epsilon(0.05)); // feet on the ground
}

TEST_CASE("a character jumps and lands") {
    PhysicsWorld world;
    make_ground(world);
    CharacterHandle hero = world.create_character({.radius = 0.4f, .height = 1.8f, .position = {0, 0, 0}});
    for (int i = 0; i < 30; ++i) world.update_character(hero, {}, false, 1.0f / 60.0f);
    world.update_character(hero, {}, true, 1.0f / 60.0f, 5.0f);
    float peak = 0;
    for (int i = 0; i < 120; ++i) {
        world.update_character(hero, {}, false, 1.0f / 60.0f);
        peak = std::max(peak, world.character_state(hero).position.y);
    }
    CHECK(peak > 1.0f);
    CHECK(world.character_state(hero).on_ground);
}

// --- M4 review regressions ----------------------------------------------------------------

TEST_CASE("end events never name a body created after the destroyed one") {
    PhysicsWorld world;
    BodyHandle ground = make_ground(world);
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 0.5f, 0}});
    simulate(world, 0.5f);
    world.destroy(ball);
    BodyHandle fresh = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {100, 50, 0}});
    world.step(1.0f / 60.0f);
    for (const ContactEvent& c : world.contacts()) {
        CHECK(c.a != fresh);
        CHECK(c.b != fresh);
    }
    (void)ground;
}

TEST_CASE("invalid shapes are refused instead of crashing") {
    PhysicsWorld world;
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::box({0, 0, 0})}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::sphere(-1.0f)}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::sphere(std::nanf(""))}).valid());
    CHECK_FALSE(world.create_character({.radius = 0.0f}).valid());
}

TEST_CASE("characters are visible to raycasts and sensors and block each other") {
    PhysicsWorld world;
    make_ground(world);
    BodyHandle sensor = world.create_body({.shape = ShapeDesc::box({0.5f, 2, 2}), .motion = MotionType::Static,
                                           .position = {2, 1, 0}, .sensor = true});
    CharacterHandle a = world.create_character({.position = {0, 0, 0}, .user_data = 7});
    CharacterHandle b = world.create_character({.position = {4, 0, 0}, .user_data = 8});
    CHECK(world.valid(a));
    const auto hit = world.raycast({0, 1, 5}, {0, 0, -1}, 10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->user_data == 7);
    bool sensed = false;
    float closest = 1e9f;
    for (int i = 0; i < 120; ++i) {
        world.update_character(a, {3, 0, 0}, false, 1.0f / 60.0f);  // walks through the sensor into b
        world.update_character(b, {0, 0, 0}, false, 1.0f / 60.0f);
        world.step(1.0f / 60.0f);
        for (const ContactEvent& c : world.contacts()) sensed |= (c.a == sensor || c.b == sensor);
        closest = std::min(closest, length(world.character_state(b).position - world.character_state(a).position));
    }
    CHECK(sensed);
    CHECK(closest > 0.75f); // never overlap (radius 0.4 each): a pushes b instead of passing through
    world.destroy(a);
    CHECK_FALSE(world.valid(a));
}

TEST_CASE("shift_origin moves every body and character, keeping velocities") {
    PhysicsWorld world;
    BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {2000, 10, -3000}});
    world.set_linear_velocity(ball, {1, 0, 0});
    BodyHandle floor = world.create_body({.shape = ShapeDesc::box({5, 0.5f, 5}), .motion = MotionType::Static,
                                          .position = {2000, 0, -3000}});
    CharacterHandle hero = world.create_character({.radius = 0.4f, .height = 1.8f, .position = {2002, 1, -3000}});
    world.shift_origin({2000, 0, -3000});
    CHECK(world.position(ball).x == doctest::Approx(0.0f));
    CHECK(world.position(ball).y == doctest::Approx(10.0f));
    CHECK(world.position(ball).z == doctest::Approx(0.0f));
    CHECK(world.position(floor).x == doctest::Approx(0.0f));
    CHECK(world.linear_velocity(ball).x == doctest::Approx(1.0f));
    CHECK(world.character_state(hero).position.x == doctest::Approx(2.0f));
    CHECK(world.character_state(hero).position.z == doctest::Approx(0.0f));
    // The ball still lands on the moved floor.
    for (int i = 0; i < 180; ++i) world.step(1.0f / 60.0f);
    CHECK(world.position(ball).y == doctest::Approx(1.0f).epsilon(0.05));
}
