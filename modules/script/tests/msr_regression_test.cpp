// Regression tests for the Master Stabilization Roadmap (docs/archive/msr/master-stabilization-roadmap.md).
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
    // Sent from on_load: top-level code may not send events (ADR-0044).
    const auto errors = f.host->load("cycle.luau", R"(
        astral.define_event("t::Payload")
        function on_load()
            local t = {}
            t.self = t
            astral.send("t::Payload", {value = t})
        end
    )");
    REQUIRE(errors.size() == 1); // on_load fails with a script error
    CHECK(errors[0].find("while the module loads") == std::string::npos);
    CHECK(f.host->failed("cycle.luau"));
    // Deep but finite nesting below the limit still converts.
    REQUIRE(f.host->load("deep.luau", R"(
        astral.define_event("t::Deep")
        function on_load()
            local t = {}
            local cur = t
            for i = 1, 20 do cur.next = {}; cur = cur.next end
            astral.send("t::Deep", {value = t})
        end
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

namespace {
std::size_t alive_count(World& w) {
    std::size_t n = 0;
    w.each<const Name>([&](Entity, const Name&) { ++n; });
    return n;
}
} // namespace

TEST_CASE("AG09: a failed load leaves the world, the types and the events as they were") {
    Fixture f;
    f.world.create(Name{"existing"});
    const std::size_t types_before = f.types.entries().size();
    const std::size_t events_before = f.events.entries().size();
    CHECK_FALSE(f.host->load("bad.luau", R"(
        astral.define_component("game::Fresh", {value = "f32"})
        astral.define_event("game::Ping")
        error("broken after the definitions")
    )").empty());
    CHECK(f.types.find("game::Fresh") == nullptr);
    CHECK(f.types.entries().size() == types_before);
    CHECK(f.events.find("game::Ping") == nullptr);
    CHECK(f.events.entries().size() == events_before);
    // The same names can be defined by a later, good load.
    CHECK(f.host->load("good.luau", R"(astral.define_component("game::Fresh", {value = "i32"}))").empty());
    REQUIRE(f.types.find("game::Fresh") != nullptr);
    CHECK(f.types.find("game::Fresh")->fields[0].kind == FieldKind::Int);
    CHECK(alive_count(f.world) == 1);
}

TEST_CASE("AG09: world changes are refused at top level; on_load runs them after the commit") {
    Fixture f;
    const auto errors = f.host->load("eager.luau", R"(astral.spawn { ["astral::Name"] = {value = "early"} })");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("on_load") != std::string::npos);
    CHECK(alive_count(f.world) == 0);
    CHECK_FALSE(f.host->load("send.luau", R"(astral.send("Bang", {strength = 1}))").empty());

    CHECK(f.host->load("late.luau", R"(
        astral.define_component("game::Tag", {n = "i32"})
        function on_load()
            astral.spawn { ["astral::Name"] = {value = "late"}, ["game::Tag"] = {n = 3} }
        end
    )").empty());
    CHECK(alive_count(f.world) == 1);
    CHECK(f.host->take_errors().empty());
}

TEST_CASE("AG09: spawn with a bad component field creates no entity") {
    Fixture f;
    REQUIRE(f.host->load("spawn.luau", R"(
        astral.system { run = function()
            local ok = pcall(function()
                astral.spawn { ["astral::Name"] = {value = "half"}, ["Counter"] = {hits = "not a number"} }
            end)
            assert(not ok)
            local ok2 = pcall(function() astral.spawn { ["astral::Name"] = {value = "x"}, ["NoSuchType"] = {} } end)
            assert(not ok2)
        end }
    )").empty());
    f.run();
    CHECK(f.host->take_errors().empty());
    CHECK(alive_count(f.world) == 0);
}

TEST_CASE("AG09: a failed reload keeps the old module and does not add its timers or listeners") {
    Fixture f;
    f.world.create(Counter{}, Name{"c"});
    REQUIRE(f.host->load("mod.luau", R"(
        astral.on("Bang", function() astral.find("c"):get("Counter").hits += 1 end)
    )").empty());
    CHECK_FALSE(f.host->load("mod.luau", R"(
        astral.on("Bang", function() astral.find("c"):get("Counter").hits += 100 end)
        astral.every(0.1, function() astral.find("c"):get("Counter").hits += 1000 end)
        error("nope")
    )").empty());
    f.world.send(Bang{1.0f});
    f.run();
    f.run();
    const Entity c = find_by_name(f.world, "c");
    REQUIRE(c.valid());
    CHECK(f.world.get<Counter>(c)->hits == 1);
}
