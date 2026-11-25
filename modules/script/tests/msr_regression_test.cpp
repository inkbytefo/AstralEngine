// Regression tests for the Master Stabilization Roadmap (docs/status/master-stabilization-roadmap.md).
#include <astral/script/script.hpp>
#include <astral/world/scene.hpp>

#include <doctest/doctest.h>

namespace {
struct Counter {
    std::int32_t hits = 0;
};
struct Bang {
    float strength = 0;
};
} // namespace
ASTRAL_REFLECT(Counter, hits)
ASTRAL_REFLECT(Bang, strength)

using namespace astral;

namespace {
struct Fixture {
    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    std::unique_ptr<ScriptHost> host;
    Fixture() {
        register_world_types(types);
        types.add<Counter>();
        events.add<Bang>();
        host = std::make_unique<ScriptHost>(ScriptEnv{world, types, events, log}, ScriptLimits{100.0});
    }
    void run(float dt = 0.5f) { host->run(ScriptPhase::Update, {.delta = dt, .fixed_delta = dt}); }
};
} // namespace

TEST_CASE("AG07: a listener that registers listeners while events are delivered is safe") {
    Fixture f;
    REQUIRE(f.host->load("grow.luau", R"(
        astral.on("Bang", function(ctx, ev)
            for i = 1, 32 do astral.on("Bang", function() end) end
            astral.find("c"):get("Counter").hits += 1
        end)
    )").empty());
    f.world.create(Counter{}, Name{"c"});
    for (int i = 0; i < 3; ++i) f.world.send(Bang{1.0f});
    f.run();
    CHECK(f.host->take_errors().empty());
    CHECK_FALSE(f.host->failed("grow.luau"));
}

TEST_CASE("AG07: a per-entity system that registers systems keeps running its own definition") {
    Fixture f;
    for (int i = 0; i < 8; ++i) f.world.create(Counter{});
    REQUIRE(f.host->load("spawner.luau", R"(
        astral.system { name = "outer", query = {"Counter"}, update = function(ctx, e, c)
            c.hits += 1
            astral.system { name = "inner", run = function() end }
        end }
    )").empty());
    f.run();
    CHECK(f.host->take_errors().empty());
    int total = 0;
    f.world.each<const Counter>([&](const Counter& c) { total += c.hits; });
    CHECK(total == 8); // each entity once: none skipped or run twice
}

TEST_CASE("AG11: after a timer callback fails, the module's other due timers do not run") {
    Fixture f;
    const Entity e = f.world.create(Counter{}, Name{"c"});
    REQUIRE(f.host->load("timers.luau", R"(
        astral.after(0.1, function() error("boom") end)
        astral.after(0.1, function() astral.find("c"):get("Counter").hits = 99 end)
    )").empty());
    f.run();
    CHECK(f.host->failed("timers.luau"));
    CHECK(f.world.get<Counter>(e)->hits == 0);
    CHECK_FALSE(f.host->take_errors().empty());
}

TEST_CASE("AG12: a table that contains itself is refused with an error, not a host stack overflow") {
    Fixture f;
    REQUIRE_FALSE(f.host->load("cycle.luau", R"(
        astral.define_event("t::Payload")
        local t = {}
        t.self = t
        astral.send("t::Payload", {value = t})
    )").empty()); // the load fails with a script error
    CHECK_FALSE(f.host->loaded("cycle.luau"));
    // Deep but finite nesting below the limit still converts.
    REQUIRE(f.host->load("deep.luau", R"(
        astral.define_event("t::Deep")
        local t = {}
        local cur = t
        for i = 1, 20 do cur.next = {}; cur = cur.next end
        astral.send("t::Deep", {value = t})
    )").empty());
}

TEST_CASE("AG10: a new script path or a re-added Script starts a new behaviour instance") {
    Fixture f;
    const Entity e = f.world.create(Counter{});
    const char* body = R"(
        astral.behaviour {
            on_start = function(self, ctx) self:get("Counter").hits += 1 end,
        }
    )";
    REQUIRE(f.host->load("a.luau", body).empty());
    REQUIRE(f.host->load("b.luau", body).empty());
    f.world.add(e, Script{"a.luau"});
    f.run();
    f.run();
    CHECK(f.world.get<Counter>(e)->hits == 1); // started once
    f.world.get<Script>(e)->path = "b.luau";
    f.run();
    CHECK(f.world.get<Counter>(e)->hits == 2); // another behaviour: its own on_start
    f.world.remove<Script>(e);
    f.run(); // the instance is forgotten
    f.world.add(e, Script{"b.luau"});
    f.run();
    CHECK(f.world.get<Counter>(e)->hits == 3); // re-added: a new instance
    CHECK(f.host->take_errors().empty());
}
