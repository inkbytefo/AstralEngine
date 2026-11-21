#include <astral/core/jobs.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using namespace astral;

namespace {
struct Position {
    float x = 0, y = 0;
};
struct Velocity {
    float x = 0, y = 0;
};
struct Health {
    int value = 100;
};
struct Tag {};

/// Counts constructions/destructions to prove no component is leaked or destroyed twice.
struct Tracked {
    static inline int alive = 0;
    std::unique_ptr<int> payload;
    explicit Tracked(int v = 0) : payload(std::make_unique<int>(v)) { ++alive; }
    Tracked(Tracked&& o) noexcept : payload(std::move(o.payload)) { ++alive; }
    Tracked& operator=(Tracked&& o) noexcept {
        payload = std::move(o.payload);
        return *this;
    }
    ~Tracked() { --alive; }
};
} // namespace

TEST_CASE("component ids are stable and distinct per type") {
    CHECK(component_id<Position>() == component_id<Position>());
    CHECK(component_id<Position>() != component_id<Velocity>());
}

TEST_CASE("created entity is alive until destroyed") {
    World world;
    Entity e = world.create();
    CHECK(world.alive(e));
    world.destroy(e);
    CHECK_FALSE(world.alive(e));
    CHECK(world.count() == 0);
}

TEST_CASE("stale entity handle does not reach a recycled slot") {
    World world;
    Entity old_e = world.create();
    world.destroy(old_e);
    Entity new_e = world.create();
    CHECK(new_e.index == old_e.index);
    CHECK_FALSE(world.alive(old_e));
    CHECK(world.get<Position>(old_e) == nullptr);
}

TEST_CASE("components can be added, read, replaced and removed") {
    World world;
    Entity e = world.create();
    world.add(e, Position{1, 2});
    REQUIRE(world.has<Position>(e));
    CHECK(world.get<Position>(e)->x == 1);
    world.add(e, Position{5, 6}); // replace
    CHECK(world.get<Position>(e)->x == 5);
    CHECK(world.remove<Position>(e));
    CHECK_FALSE(world.has<Position>(e));
    CHECK_FALSE(world.remove<Position>(e));
}

TEST_CASE("moving between archetypes keeps other component values") {
    World world;
    Entity e = world.create(Position{3, 4}, Health{42});
    world.add(e, Velocity{1, 1});
    CHECK(world.get<Position>(e)->y == 4);
    CHECK(world.get<Health>(e)->value == 42);
    world.remove<Position>(e);
    CHECK(world.get<Health>(e)->value == 42);
    CHECK(world.get<Velocity>(e)->x == 1);
}

TEST_CASE("swap-remove keeps the moved entity addressable") {
    World world;
    Entity a = world.create(Health{1});
    Entity b = world.create(Health{2});
    Entity c = world.create(Health{3});
    world.destroy(a); // c moves into a's row
    CHECK(world.get<Health>(b)->value == 2);
    CHECK(world.get<Health>(c)->value == 3);
}

TEST_CASE("non-trivial components are constructed and destroyed exactly once") {
    Tracked::alive = 0;
    {
        World world;
        std::vector<Entity> es;
        for (int i = 0; i < 100; ++i) es.push_back(world.create(Tracked{i}, Health{}));
        for (int i = 0; i < 100; i += 2) world.add(es[i], Velocity{}); // archetype moves
        for (int i = 0; i < 100; i += 3) world.destroy(es[i]);
        CHECK(Tracked::alive == world.count());
        CHECK(*world.get<Tracked>(es[1])->payload == 1);
    }
    CHECK(Tracked::alive == 0); // world destructor released the rest
}

TEST_CASE("each visits exactly the entities that have all requested components") {
    World world;
    for (int i = 0; i < 10; ++i) world.create(Position{}, Velocity{1, 0});
    for (int i = 0; i < 5; ++i) world.create(Position{});
    world.create(Velocity{});

    int visited = 0;
    world.each<Position, Velocity>([&](Position& p, const Velocity& v) {
        p.x += v.x;
        ++visited;
    });
    CHECK(visited == 10);
    CHECK(world.count<Position>() == 15);
    CHECK(world.count<Position, Velocity>() == 10);
}

TEST_CASE("each can receive the entity") {
    World world;
    Entity e = world.create(Health{7});
    Entity seen{};
    world.each<Health>([&](Entity entity, Health&) { seen = entity; });
    CHECK(seen == e);
}

TEST_CASE("empty tag components can be queried") {
    World world;
    world.create(Tag{}, Health{});
    world.create(Health{});
    CHECK(world.count<Tag>() == 1);
}

TEST_CASE("commands defer structural changes made during each") {
    World world;
    for (int i = 0; i < 10; ++i) world.create(Health{i});
    world.each<Health>([&](Entity e, Health& h) {
        if (h.value % 2 == 0) world.commands().destroy(e);
        else world.commands().add(e, Velocity{});
        world.commands().spawn(Position{});
    });
    CHECK(world.count() == 10); // nothing applied yet
    world.apply_commands();
    CHECK(world.count<Health>() == 5);
    CHECK(world.count<Health, Velocity>() == 5);
    CHECK(world.count<Position>() == 10);
}

TEST_CASE("parallel_each processes every entity exactly once") {
    World world;
    JobSystem jobs(4);
    for (int i = 0; i < 20'000; ++i) world.create(Position{}, Velocity{1, 2});
    for (int i = 0; i < 5'000; ++i) world.create(Position{}, Velocity{1, 2}, Health{});
    world.parallel_each<Position, const Velocity>(jobs, [](Position& p, const Velocity& v) {
        p.x += v.x;
        p.y += v.y;
    });
    int wrong = 0;
    world.each<Position>([&](Position& p) { wrong += (p.x != 1.0f || p.y != 2.0f) ? 1 : 0; });
    CHECK(wrong == 0);
}

TEST_CASE("entity wrapper offers a fluent API") {
    World world;
    EntityRef player = world.spawn();
    player.add(Position{1, 1}).add(Health{50});
    CHECK(player.get<Health>()->value == 50);
    CHECK(player.has<Position>());
    player.destroy();
    CHECK(world.count() == 0);
}

TEST_CASE("world resources are unique per type") {
    World world;
    struct Gravity {
        float value = -9.8f;
    };
    world.set_resource(Gravity{-3.0f});
    REQUIRE(world.resource<Gravity>() != nullptr);
    CHECK(world.resource<Gravity>()->value == -3.0f);
    CHECK(world.resource<Health>() == nullptr);
}
