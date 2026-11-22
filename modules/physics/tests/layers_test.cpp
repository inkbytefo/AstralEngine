// Collision layers, query filters, shape casts, overlaps, surfaces (M12.1).
#include <astral/physics/physics.hpp>
#include <doctest/doctest.h>

#include <algorithm>

using namespace astral;

namespace {
void simulate(PhysicsWorld& world, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) world.step(1.0f / 60.0f);
}
BodyHandle ground(PhysicsWorld& world, std::uint8_t layer = 0) {
    return world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}, .layer = layer});
}
} // namespace

TEST_CASE("layers that do not collide pass through each other; the matrix is symmetric and can change") {
    PhysicsWorld world;
    constexpr std::uint8_t ghosts = 3;
    ground(world);
    CHECK(world.layers_collide(0, ghosts)); // everything collides by default
    world.set_layer_collision(0, ghosts, false);
    CHECK_FALSE(world.layers_collide(ghosts, 0));
    const BodyHandle ghost = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 3, 0}, .layer = ghosts});
    const BodyHandle solid = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {4, 3, 0}});
    simulate(world, 3.0f);
    CHECK(world.position(ghost).y < -5.0f);                              // fell through the ground
    CHECK(world.position(solid).y == doctest::Approx(0.5f).epsilon(0.1)); // the default layer stands on it
    // Ghosts still collide with each other (same layer).
    CHECK(world.layers_collide(ghosts, ghosts));
}

TEST_CASE("two static layers never test against each other, a moving body does") {
    PhysicsWorld world;
    const BodyHandle a = world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 0, 0}, .layer = 1});
    const BodyHandle b = world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 0, 0}, .layer = 2});
    simulate(world, 0.2f);
    CHECK(world.contacts().empty()); // overlapping statics: nothing to report
    CHECK(world.valid(a));
    CHECK(world.valid(b));
}

TEST_CASE("query filters: layer masks, ignore and sensors") {
    PhysicsWorld world;
    const BodyHandle wall_a = world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 0, 5}, .layer = 1, .user_data = 11});
    world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 0, 10}, .layer = 2, .user_data = 22});
    world.create_body({.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .position = {0, 0, 15}, .layer = 2, .sensor = true, .user_data = 33});
    const auto first = world.raycast({0, 0, 0}, {0, 0, 1}, 50.0f);
    REQUIRE(first);
    CHECK(first->user_data == 11);
    const auto only_layer_2 = world.raycast({0, 0, 0}, {0, 0, 1}, 50.0f, QueryFilter{.layers = layer_bit(2)});
    REQUIRE(only_layer_2);
    CHECK(only_layer_2->user_data == 22); // layer 1 is not asked for
    const auto ignoring = world.raycast({0, 0, 0}, {0, 0, 1}, 50.0f, QueryFilter{.layers = layer_bit(1)});
    REQUIRE(ignoring);
    const auto skipped = world.raycast({0, 0, 0}, {0, 0, 1}, 50.0f, QueryFilter{.ignore = wall_a});
    REQUIRE(skipped);
    CHECK(skipped->user_data == 22); // the nearer wall is ignored
    const auto sensors = world.raycast({0, 0, 12}, {0, 0, 1}, 50.0f, QueryFilter{.layers = layer_bit(2), .include_sensors = true});
    REQUIRE(sensors);
    CHECK(sensors->user_data == 33);
    CHECK_FALSE(world.raycast({0, 0, 12}, {0, 0, 1}, 50.0f, QueryFilter{.layers = layer_bit(2)}).has_value()); // sensors are skipped
}

TEST_CASE("shape casts report the first thing a swept shape hits") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({2, 2, 0.5f}), .motion = MotionType::Static, .position = {0, 0, 10}, .user_data = 7});
    const auto hit = world.shape_cast(ShapeDesc::sphere(0.5f), {0, 0, 0}, {}, {0, 0, 20});
    REQUIRE(hit);
    CHECK(hit->user_data == 7);
    CHECK(hit->distance == doctest::Approx(9.0f).epsilon(0.02)); // 10 - 0.5 (wall half depth) - 0.5 (sphere radius)
    CHECK(hit->normal.z < -0.9f);
    CHECK_FALSE(world.shape_cast(ShapeDesc::sphere(0.5f), {5, 0, 0}, {}, {0, 0, 20}).has_value()); // misses the wall
    CHECK_FALSE(world.shape_cast(ShapeDesc::sphere(0.5f), {0, 0, 0}, {}, {0, 0, 5}).has_value());  // too short
    CHECK_FALSE(world.shape_cast(ShapeDesc::sphere(0.5f), {0, 0, 0}, {}, {0, 0, 0}).has_value());   // no movement
}

TEST_CASE("overlap lists the bodies inside a shape, honouring layers and sensors") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::sphere(0.5f), .motion = MotionType::Static, .position = {1, 0, 0}, .user_data = 1});
    world.create_body({.shape = ShapeDesc::sphere(0.5f), .motion = MotionType::Static, .position = {-1, 0, 0}, .layer = 2, .user_data = 2});
    world.create_body({.shape = ShapeDesc::sphere(0.5f), .motion = MotionType::Static, .position = {30, 0, 0}, .user_data = 3});
    world.create_body({.shape = ShapeDesc::sphere(0.5f), .motion = MotionType::Static, .position = {0, 1, 0}, .sensor = true, .user_data = 4});
    auto ids = [](const std::vector<OverlapHit>& hits) {
        std::vector<std::uint64_t> out;
        for (const OverlapHit& h : hits) out.push_back(h.user_data);
        std::sort(out.begin(), out.end());
        return out;
    };
    CHECK(ids(world.overlap(ShapeDesc::sphere(2.0f), {0, 0, 0})) == std::vector<std::uint64_t>{1, 2});
    CHECK(ids(world.overlap(ShapeDesc::sphere(2.0f), {0, 0, 0}, {}, QueryFilter{.layers = layer_bit(2)})) == std::vector<std::uint64_t>{2});
    CHECK(ids(world.overlap(ShapeDesc::sphere(2.0f), {0, 0, 0}, {}, QueryFilter{.include_sensors = true})) == std::vector<std::uint64_t>{1, 2, 4});
    CHECK(world.overlap(ShapeDesc::box({0.1f, 0.1f, 0.1f}), {10, 10, 10}).empty());
}

TEST_CASE("surface ids travel with hits and contacts") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}, .surface = 4});
    const auto hit = world.raycast({0, 5, 0}, {0, -1, 0}, 20.0f);
    REQUIRE(hit);
    CHECK(hit->surface == 4);
    const BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 2, 0}, .surface = 9});
    bool saw = false;
    for (int i = 0; i < 120 && !saw; ++i) {
        world.step(1.0f / 60.0f);
        for (const ContactEvent& c : world.contacts()) {
            if (c.type != ContactEvent::Type::Begin) continue;
            saw = true;
            CHECK(std::max(c.surface_a, c.surface_b) == 9);
            CHECK(std::min(c.surface_a, c.surface_b) == 4);
            CHECK_FALSE(c.sensor);
        }
    }
    CHECK(saw);
    (void)ball;
}

TEST_CASE("a sensor contact says so, so triggers can be told from collisions") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({2, 2, 2}), .motion = MotionType::Static, .position = {0, 1, 0}, .sensor = true});
    world.create_body({.shape = ShapeDesc::sphere(0.3f), .position = {0, 6, 0}});
    bool trigger = false;
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        for (const ContactEvent& c : world.contacts()) trigger = trigger || (c.type == ContactEvent::Type::Begin && c.sensor);
    }
    CHECK(trigger);
}
