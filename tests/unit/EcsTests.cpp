#include <doctest/doctest.h>

#include "Astral/Engine/Ecs/Registry.hpp"

#include <string>

using namespace Astral;

namespace {
struct Position { float x = 0, y = 0; };
struct Velocity { float x = 0, y = 0; };
struct Tag { std::string name; };
} // namespace

TEST_CASE("Registry creates, validates and recycles entities") {
    Registry reg;
    Entity a = reg.Create();
    Entity b = reg.Create();
    CHECK(reg.Valid(a));
    CHECK(reg.AliveCount() == 2);

    reg.Destroy(a);
    CHECK_FALSE(reg.Valid(a));
    Entity c = reg.Create();
    CHECK(c.index == a.index);
    CHECK_FALSE(reg.Valid(a)); // stale handle stays invalid
    CHECK(reg.Valid(c));
    CHECK(reg.FromIndex(c.index) == c);
    CHECK(reg.Entities().size() == 2);
    (void)b;
}

TEST_CASE("Components can be added, read, replaced and removed") {
    Registry reg;
    Entity e = reg.Create();
    reg.Add<Position>(e, Position{1, 2});
    CHECK(reg.Has<Position>(e));
    CHECK(reg.Get<Position>(e).y == 2);

    reg.Add<Position>(e, Position{5, 6}); // replaces
    CHECK(reg.Count<Position>() == 1);
    CHECK(reg.Get<Position>(e).x == 5);

    reg.Remove<Position>(e);
    CHECK_FALSE(reg.Has<Position>(e));
    CHECK(reg.TryGet<Velocity>(e) == nullptr);
}

TEST_CASE("Destroying an entity removes all of its components") {
    Registry reg;
    Entity e = reg.Create();
    reg.Add<Position>(e);
    reg.Add<Tag>(e, Tag{"player"});
    reg.Destroy(e);
    CHECK(reg.Count<Position>() == 0);
    CHECK(reg.Count<Tag>() == 0);
}

TEST_CASE("Each visits exactly the entities that have every component") {
    Registry reg;
    for (int i = 0; i < 10; ++i) {
        Entity e = reg.Create();
        reg.Add<Position>(e, Position{float(i), 0});
        if (i % 2 == 0) reg.Add<Velocity>(e, Velocity{1, 0});
    }
    int visited = 0;
    reg.Each<Position, Velocity>([&](Entity, Position& p, Velocity& v) {
        p.x += v.x;
        ++visited;
    });
    CHECK(visited == 5);

    float total = 0;
    reg.Each<Position>([&](Entity, Position& p) { total += p.x; });
    CHECK(total == doctest::Approx(45 + 5));
}

TEST_CASE("Swap-and-pop removal keeps the remaining components addressable") {
    Registry reg;
    Entity a = reg.Create(), b = reg.Create(), c = reg.Create();
    reg.Add<Position>(a, Position{1, 0});
    reg.Add<Position>(b, Position{2, 0});
    reg.Add<Position>(c, Position{3, 0});
    reg.Remove<Position>(a);
    CHECK(reg.Get<Position>(b).x == 2);
    CHECK(reg.Get<Position>(c).x == 3);
}
