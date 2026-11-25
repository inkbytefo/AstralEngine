// Convex hulls, triangle meshes, height fields, compound shapes (M12.2).
#include <astral/physics/physics.hpp>
#include <doctest/doctest.h>

#include <cmath>

using namespace astral;

namespace {
void simulate(PhysicsWorld& world, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) world.step(1.0f / 60.0f);
}
} // namespace

TEST_CASE("a convex hull body falls and comes to rest on a hull face") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}});
    // A square pyramid, base 2 x 2, height 1.5, apex at y = 1.5 (centre of the base at the origin).
    const std::vector<Vec3> pyramid{{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}, {0, 1.5f, 0}};
    const BodyHandle body = world.create_body({.shape = ShapeDesc::convex_hull(pyramid), .position = {0, 4, 0}, .rotation = Quat::from_axis_angle({1, 0, 0}, 2.6f)});
    REQUIRE(body.valid());
    simulate(world, 5.0f);
    CHECK(length(world.linear_velocity(body)) < 0.1f);
    CHECK(world.position(body).y > 0.1f);
    CHECK(world.position(body).y < 1.5f); // lying on a face, not floating
}

TEST_CASE("a triangle mesh is a static ground that balls roll down") {
    PhysicsWorld world;
    // A ramp: two triangles rising along +z (slope 20 degrees), 20 m wide.
    const float rise = 10.0f * std::tan(radians(20.0f));
    const std::vector<Vec3> vertices{{-10, 0, 0}, {10, 0, 0}, {10, rise, 10}, {-10, rise, 10}};
    const std::vector<std::uint32_t> indices{0, 2, 1, 0, 3, 2}; // counter-clockwise seen from above
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}}); // the floor below the ramp's foot
    const BodyHandle ramp = world.create_body({.shape = ShapeDesc::mesh(vertices, indices), .motion = MotionType::Static});
    REQUIRE(ramp.valid());
    const auto hit = world.raycast({0, 10, 5}, {0, -1, 0}, 20.0f);
    REQUIRE(hit);
    CHECK(hit->position.y == doctest::Approx(5.0f * std::tan(radians(20.0f))).epsilon(0.02));
    CHECK(hit->normal.y > 0.9f);
    const BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0, 3.2f, 5}});
    simulate(world, 3.0f);
    CHECK(world.position(ball).z < 3.0f); // rolled down the slope (towards z = 0)
    CHECK(world.position(ball).y > 0.4f);  // and did not fall through
}

TEST_CASE("a height field is a ground with hills") {
    PhysicsWorld world;
    constexpr std::uint32_t n = 32;
    std::vector<float> heights(n * n, 0.0f);
    for (std::uint32_t z = 0; z < n; ++z) {
        for (std::uint32_t x = 0; x < n; ++x) {
            const float dx = static_cast<float>(x) - 16.0f, dz = static_cast<float>(z) - 16.0f;
            heights[z * n + x] = std::max(0.0f, 3.0f - 0.5f * std::sqrt(dx * dx + dz * dz)); // a cone hill in the middle
        }
    }
    const BodyHandle ground = world.create_body({.shape = ShapeDesc::height_field(heights, n, 1.0f), .motion = MotionType::Static});
    REQUIRE(ground.valid());
    const auto top = world.raycast({0, 20, 0}, {0, -1, 0}, 40.0f); // over the hill's peak (the field is centred on the body)
    REQUIRE(top);
    CHECK(top->position.y == doctest::Approx(3.0f).epsilon(0.1));
    const auto far_corner = world.raycast({12, 20, 12}, {0, -1, 0}, 40.0f);
    REQUIRE(far_corner);
    CHECK(std::abs(far_corner->position.y) < 0.01f);
    const BodyHandle ball = world.create_body({.shape = ShapeDesc::sphere(0.5f), .position = {0.2f, 8, 0.1f}});
    simulate(world, 4.0f);
    CHECK(world.position(ball).y > 0.4f); // did not fall through
    CHECK(world.position(ball).y < 3.6f);
}

TEST_CASE("a compound shape moves as one body: a dumbbell's mass is in both ends") {
    PhysicsWorld world;
    world.create_body({.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static, .position = {0, -0.5f, 0}});
    ShapeDesc dumbbell = ShapeDesc::compound({{{-1, 0, 0}, {}, ShapeDesc::sphere(0.5f)}, {{1, 0, 0}, {}, ShapeDesc::sphere(0.5f)}, {{0, 0, 0}, {}, ShapeDesc::box({0.8f, 0.1f, 0.1f})}});
    const BodyHandle body = world.create_body({.shape = dumbbell, .position = {0, 3, 0}, .rotation = Quat::from_axis_angle({0, 0, 1}, 0.3f)});
    REQUIRE(body.valid());
    simulate(world, 4.0f);
    CHECK(world.position(body).y == doctest::Approx(0.5f).epsilon(0.15)); // rests on its two spheres
    CHECK(length(world.linear_velocity(body)) < 0.1f);
    CHECK(world.overlap(ShapeDesc::sphere(0.2f), {1, 0.5f, 0}).size() == 1); // the right-hand sphere is solid there
}

TEST_CASE("bad shapes are refused with an error, not a crash") {
    PhysicsWorld world;
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::convex_hull({{0, 0, 0}, {1, 0, 0}})}).valid());                // too few points
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::convex_hull({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}})}).valid()); // flat: no volume
    const std::vector<Vec3> tri{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::mesh(tri, {0, 1, 2}), .motion = MotionType::Dynamic}).valid()); // meshes cannot move
    CHECK(world.create_body({.shape = ShapeDesc::mesh(tri, {0, 1, 2}), .motion = MotionType::Static}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::mesh(tri, {0, 1, 7}), .motion = MotionType::Static}).valid());   // index out of range
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::mesh(tri, {0, 1}), .motion = MotionType::Static}).valid());      // not a multiple of 3
    std::vector<float> heights(16 * 16, 0.0f);
    heights[5] = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::height_field(heights, 16, 1.0f), .motion = MotionType::Static}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::height_field(std::vector<float>(10, 0.0f), 16, 1.0f), .motion = MotionType::Static}).valid()); // wrong count
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::compound({})}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::compound({{{0, 0, 0}, {}, ShapeDesc::sphere(-1.0f)}})}).valid());
    CHECK_FALSE(world.shape_cast(ShapeDesc::mesh(tri, {0, 1, 2}), {0, 0, 0}, {}, {0, 0, 5}).has_value()); // meshes cannot be swept
}

TEST_CASE("PH06: a hull is accepted whatever the order of its points, duplicates included") {
    PhysicsWorld world;
    // A tetrahedron listed as O, X, O, Y, Z: the old check picked the plane O-Y-Z and missed X.
    const std::vector<Vec3> points = {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    CHECK(world.create_body({.shape = ShapeDesc::convex_hull(points), .position = {0, 2, 0}}).valid());
    // Still refused: every point on one line, or one point repeated.
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::convex_hull({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}})}).valid());
    CHECK_FALSE(world.create_body({.shape = ShapeDesc::convex_hull({{1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}})}).valid());
}
