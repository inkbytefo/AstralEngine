// Regression tests for the M2 review findings.
#include <astral/core/jobs.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

#include <string>

using namespace astral;

astral::ComponentId state_id_a();
astral::ComponentId state_id_b();
void add_state_a(World&, Entity, int);
int get_state_a(World&, Entity);
void add_state_b(World&, Entity, const char*);
std::string get_state_b(World&, Entity);

TEST_CASE("same-named components from different translation units are distinct") {
    CHECK(state_id_a() != state_id_b());
    World world;
    Entity e = world.create();
    add_state_a(world, e, 7);
    add_state_b(world, e, "a string long enough to live on the heap, not in SSO");
    CHECK(get_state_a(world, e) == 7);
    CHECK(get_state_b(world, e) == "a string long enough to live on the heap, not in SSO");
}

TEST_CASE("adding a copy of a component that lives in the world is safe when storage grows") {
    struct Pos {
        float x = 0;
    };
    struct Vel {
        std::string tag;
    };
    World world;
    Entity src{};
    for (int i = 0; i < 64; ++i) src = world.create(Pos{}, Vel{"velocity component with a heap string"});
    Entity target = world.create(Pos{});
    // The argument points into the {Pos, Vel} column, which grows during the move.
    world.add(target, *world.get<Vel>(src));
    CHECK(world.get<Vel>(target)->tag == "velocity component with a heap string");
    Entity clone = world.create(*world.get<Vel>(src), Pos{});
    CHECK(world.get<Vel>(clone)->tag == "velocity component with a heap string");
}

TEST_CASE("commands can be recorded from parallel_each") {
    struct Health {
        int value;
    };
    World world;
    JobSystem jobs(4);
    for (int i = 0; i < 20'000; ++i) world.create(Health{i});
    world.parallel_each<const Health>(jobs, [&](Entity e, const Health& h) {
        if (h.value % 2 == 0) world.commands().destroy(e);
    });
    world.apply_commands();
    CHECK(world.count<Health>() == 10'000);
}

TEST_CASE("deep hierarchies are resolved exactly, without a depth cap") {
    World world;
    Entity parent = world.create(Transform{.position = {1, 0, 0}});
    Entity last = parent;
    for (int i = 0; i < 200; ++i) last = world.create(Transform{.position = {1, 0, 0}}, Parent{last});
    update_transforms(world);
    CHECK(world.get<WorldTransform>(last)->matrix[3].x == doctest::Approx(201.0f));
}

TEST_CASE("a parent cycle treats the cycle entry as a root") {
    World world;
    Entity a = world.create(Transform{.position = {1, 0, 0}});
    Entity b = world.create(Transform{.position = {2, 0, 0}}, Parent{a});
    world.add(a, Parent{b});
    update_transforms(world);
    const float ax = world.get<WorldTransform>(a)->matrix[3].x;
    const float bx = world.get<WorldTransform>(b)->matrix[3].x;
    CHECK((ax == doctest::Approx(1.0f) || ax == doctest::Approx(3.0f)));
    CHECK((bx == doctest::Approx(2.0f) || bx == doctest::Approx(3.0f)));
}
