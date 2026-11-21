#include <astral/world/events.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <vector>

using namespace astral;

namespace {
struct Pos { float x = 0; };
struct Vel { float v = 0; };
struct Frozen {};
struct Hit { int damage = 0; };

std::vector<Entity> collect(auto&& query) {
    std::vector<Entity> out;
    query.each([&](Entity e, auto&&...) { out.push_back(e); });
    std::sort(out.begin(), out.end(), [](Entity a, Entity b) { return a.index < b.index; });
    return out;
}
} // namespace

TEST_CASE("query filters: with and without") {
    World w;
    const Entity a = w.create(Pos{}, Vel{});
    const Entity b = w.create(Pos{}, Vel{}, Frozen{});
    const Entity c = w.create(Pos{});
    CHECK(collect(w.query<Pos>()) == std::vector<Entity>{a, b, c});
    CHECK(collect(w.query<Pos>().with<Vel>()) == std::vector<Entity>{a, b});
    CHECK(collect(w.query<Pos, const Vel>().without<Frozen>()) == std::vector<Entity>{a});
    CHECK(w.query<Pos>().without<Vel>().count() == 1);
    CHECK(w.query<Pos>().without<Frozen>().without<Vel>().count() == 1);
}

TEST_CASE("added and changed: since the last clear_trackers()") {
    World w;
    const Entity a = w.create(Pos{1});
    const Entity b = w.create(Pos{2});
    CHECK(w.query<const Pos>().added<Pos>().count() == 2); // everything is new at first
    w.clear_trackers();
    CHECK(w.query<const Pos>().added<Pos>().count() == 0);
    CHECK(w.query<const Pos>().changed<Pos>().count() == 0);

    // Reading through const does not count as a change; mutable access does.
    w.each<const Pos>([](const Pos&) {});
    CHECK(w.query<const Pos>().changed<Pos>().count() == 0);
    w.get<Pos>(b)->x = 5;
    CHECK(collect(w.query<const Pos>().changed<Pos>()) == std::vector<Entity>{b});
    (void)w.get<const Pos>(a);
    CHECK(w.query<const Pos>().changed<Pos>().count() == 1);

    // Adding a component marks it added (and changed); others keep their ticks across the move.
    w.clear_trackers();
    w.add(a, Vel{});
    CHECK(collect(w.query<const Vel>().added<Vel>()) == std::vector<Entity>{a});
    CHECK(w.query<const Pos>().changed<Pos>().count() == 0);
    // Replacing a value with add() is a change, not an addition.
    w.clear_trackers();
    w.add(a, Vel{3});
    CHECK(w.query<const Vel>().added<Vel>().count() == 0);
    CHECK(w.query<const Vel>().changed<Vel>().count() == 1);
}

TEST_CASE("mutable iteration marks every visited row changed") {
    World w;
    for (int i = 0; i < 100; ++i) w.create(Pos{}, Vel{});
    w.create(Pos{}); // not visited below
    w.clear_trackers();
    w.each<Pos, const Vel>([](Pos& p, const Vel&) { p.x += 1; });
    CHECK(w.query<const Pos>().changed<Pos>().count() == 100);
    CHECK(w.query<const Vel>().changed<Vel>().count() == 0);
}

TEST_CASE("change ticks survive archetype moves and swap-removes") {
    World w;
    std::vector<Entity> es;
    for (int i = 0; i < 10; ++i) es.push_back(w.create(Pos{static_cast<float>(i)}));
    w.clear_trackers();
    w.get<Pos>(es[9])->x = 99; // last row: it gets swapped into the hole below
    w.destroy(es[2]);
    w.add(es[5], Frozen{}); // moves es[5] (unchanged) to another archetype
    CHECK(collect(w.query<const Pos>().changed<Pos>()) == std::vector<Entity>{es[9]});
}

TEST_CASE("a change baseline lets each system see changes since it last ran") {
    World w;
    const Entity e = w.create(Pos{});
    Tick system_last_run = 0;
    auto run_system = [&] {
        w.set_change_baseline(system_last_run);
        const std::size_t seen = w.query<const Pos>().changed<Pos>().count();
        system_last_run = w.tick();
        w.advance_tick();
        return seen;
    };
    CHECK(run_system() == 1); // first run sees the creation
    CHECK(run_system() == 0);
    w.get<Pos>(e)->x = 1;
    w.advance_tick();
    CHECK(run_system() == 1);
    CHECK(run_system() == 0);
}

TEST_CASE("filtered parallel iteration") {
    World w;
    JobSystem jobs(2);
    for (int i = 0; i < 5000; ++i) {
        const Entity e = w.create(Pos{}, Vel{1});
        if (i % 2) w.add(e, Frozen{});
    }
    std::atomic<int> visited{0};
    w.query<Pos, const Vel>().without<Frozen>().parallel_each(jobs, [&](Pos& p, const Vel& v) {
        p.x += v.v;
        ++visited;
    });
    CHECK(visited == 2500);
}

TEST_CASE("events: each reader sees each event once; events live for two updates") {
    World w;
    EventReader<Hit> early, late;
    w.send(Hit{1});
    w.send(Hit{2});
    std::vector<int> got;
    CHECK(early.read(w, [&](const Hit& h) { got.push_back(h.damage); }) == 2);
    CHECK(got == std::vector<int>{1, 2});
    CHECK(early.read(w, [&](const Hit&) {}) == 0); // already seen
    w.update_events();
    w.send(Hit{3});
    got.clear();
    CHECK(late.read(w, [&](const Hit& h) { got.push_back(h.damage); }) == 3); // previous + current
    CHECK(got == std::vector<int>{1, 2, 3});
    w.update_events();
    w.update_events(); // Hit{3} is two updates old now: gone
    got.clear();
    EventReader<Hit> fresh;
    CHECK(fresh.read(w, [&](const Hit& h) { got.push_back(h.damage); }) == 0);
    CHECK(early.read(w, [&](const Hit& h) { got.push_back(h.damage); }) == 0); // missed 3: too slow
    // Reading a type nobody ever sent is fine.
    EventReader<Pos> none;
    CHECK(none.read(w, [](const Pos&) {}) == 0);
}
