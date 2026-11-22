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

TEST_CASE("parallel update_transforms matches serial propagation across deep hierarchies and cycles") {
    auto populate = [](World& w) {
        std::vector<Entity> all;
        all.reserve(2400);
        for (int i = 0; i < 1200; ++i) {
            const float fi = static_cast<float>(i);
            Entity root = w.create(Transform{
                .position = {fi * 0.5f, -fi * 0.25f, 1.0f},
                .rotation = Quat::from_axis_angle({0, 1, 0}, radians(fi * 3.0f)),
                .scale = {1.0f, 1.5f, 0.75f},
            });
            Entity child = w.create(
                Transform{
                    .position = {1.0f, 2.0f, -0.5f},
                    .rotation = Quat::from_axis_angle({1, 0, 0}, radians(15.0f)),
                },
                Parent{root});
            all.push_back(root);
            all.push_back(child);
            if ((i % 4) == 0) {
                Entity grand = w.create(Transform{.position = {0.0f, 0.5f, 2.0f}}, Parent{child});
                all.push_back(grand);
            }
        }
        // Include a destroyed parent and a 2-node cycle to exercise fallback paths under JobSystem.
        Entity temp_root = w.create(Transform{.position = {9, 9, 9}});
        Entity orphan = w.create(Transform{.position = {3, 4, 5}}, Parent{temp_root});
        w.destroy(temp_root);
        all.push_back(orphan);

        Entity ca = w.create(Transform{.position = {1, 1, 1}});
        Entity cb = w.create(Transform{.position = {2, 2, 2}}, Parent{ca});
        w.add(ca, Parent{cb});
        all.push_back(ca);
        all.push_back(cb);
        return all;
    };

    World serial_world;
    World parallel_world;
    const auto serial_entities = populate(serial_world);
    const auto parallel_entities = populate(parallel_world);

    JobSystem jobs(3);
    update_transforms(serial_world, nullptr);
    update_transforms(parallel_world, &jobs);

    REQUIRE(serial_entities.size() == parallel_entities.size());
    for (std::size_t i = 0; i < serial_entities.size(); ++i) {
        REQUIRE(serial_world.has<WorldTransform>(serial_entities[i]));
        REQUIRE(parallel_world.has<WorldTransform>(parallel_entities[i]));
        CHECK(near(world_position(serial_world, serial_entities[i]),
                   world_position(parallel_world, parallel_entities[i])));
    }
}
