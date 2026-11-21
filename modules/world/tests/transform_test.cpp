#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {
Vec3 world_position(World& w, Entity e) {
    const Mat4& m = w.get<WorldTransform>(e)->matrix;
    return {m[3].x, m[3].y, m[3].z};
}
bool near(Vec3 a, Vec3 b) {
    return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f && std::abs(a.z - b.z) < 1e-4f;
}
} // namespace

TEST_CASE("root transform becomes its world transform") {
    World world;
    Entity e = world.create(Transform{.position = {1, 2, 3}});
    update_transforms(world);
    REQUIRE(world.has<WorldTransform>(e));
    CHECK(near(world_position(world, e), {1, 2, 3}));
}

TEST_CASE("children inherit their parent's transform through the hierarchy") {
    World world;
    Entity root = world.create(Transform{.position = {10, 0, 0}});
    Entity child = world.create(Transform{.position = {1, 0, 0}}, Parent{root});
    Entity grandchild = world.create(Transform{.position = {0, 1, 0}}, Parent{child});
    update_transforms(world);
    CHECK(near(world_position(world, child), {11, 0, 0}));
    CHECK(near(world_position(world, grandchild), {11, 1, 0}));
}

TEST_CASE("parent rotation and scale apply to children") {
    World world;
    Entity root = world.create(Transform{.position = {0, 0, 0},
                                         .rotation = Quat::from_axis_angle({0, 1, 0}, radians(90)),
                                         .scale = {2, 2, 2}});
    Entity child = world.create(Transform{.position = {1, 0, 0}}, Parent{root});
    update_transforms(world);
    CHECK(near(world_position(world, child), {0, 0, -2}));
}

TEST_CASE("a child whose parent was destroyed is treated as a root") {
    World world;
    Entity root = world.create(Transform{.position = {5, 0, 0}});
    Entity child = world.create(Transform{.position = {1, 0, 0}}, Parent{root});
    world.destroy(root);
    update_transforms(world);
    CHECK(near(world_position(world, child), {1, 0, 0}));
}

TEST_CASE("parent cycles do not hang") {
    World world;
    Entity a = world.create(Transform{});
    Entity b = world.create(Transform{}, Parent{a});
    world.add(a, Parent{b});
    update_transforms(world); // must terminate
    CHECK(world.has<WorldTransform>(a));
}
