// Luau scripting (M11.2-M11.4): systems, components, events, behaviours, timers, failure containment.
#include <astral/script/script.hpp>
#include <astral/world/scene.hpp>

#include <doctest/doctest.h>

namespace {
enum class Mode : std::uint8_t { Idle, Run };
struct Mover {
    astral::Vec3 velocity{1, 0, 0};
    float speed = 2.0f;
    std::int32_t steps = 0;
    bool active = true;
    Mode mode = Mode::Idle;
    astral::Vec2 size{1, 2};
    astral::Entity target{};
    std::string label = "mover";
};
struct Frozen {};
struct Pinged {
    float strength = 0;
    astral::Vec3 where{};
    astral::Entity who{};
};
} // namespace
ASTRAL_REFLECT_ENUM(Mode, Idle, Run)
ASTRAL_REFLECT(Mover, velocity, speed, steps, active, mode, size, target, label)
ASTRAL_REFLECT_EMPTY(Frozen)
ASTRAL_REFLECT(Pinged, strength, where, who)

using namespace astral;

namespace {
struct Fixture {
    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    std::unique_ptr<ScriptHost> host;
    Fixture(double budget_ms = 100.0) {
        register_world_types(types);
        types.add<Mover>();
        types.add<Frozen>();
        events.add<Pinged>();
        host = std::make_unique<ScriptHost>(ScriptEnv{world, types, events, log}, ScriptLimits{budget_ms});
    }
    void run(ScriptPhase phase = ScriptPhase::Update, float dt = 0.5f) { host->run(phase, {.delta = dt, .fixed_delta = dt}); }
    std::string first_error() {
        const auto errors = host->take_errors();
        return errors.empty() ? std::string{} : errors.front();
    }
};
bool contains(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }
} // namespace

TEST_CASE("a system reads and writes components through proxies; vectors do arithmetic") {
    Fixture f;
    const Entity e = f.world.create(Mover{});
    const auto errors = f.host->load("move.luau", R"(
        astral.system {
            name = "move", phase = "Update", query = {"Mover"},
            update = function(ctx, e, m)
                m.velocity = m.velocity * 2 + vector.create(0, 1, 0)
                m.steps = m.steps + 1
                m.speed = ctx.delta
            end,
        }
    )");
    REQUIRE(errors.empty());
    f.run();
    f.run();
    const Mover* m = f.world.get<Mover>(e);
    CHECK(m->steps == 2);
    CHECK(m->speed == doctest::Approx(0.5f));
    CHECK(m->velocity.x == doctest::Approx(4.0f)); // 1 -> 2 -> 4
    CHECK(m->velocity.y == doctest::Approx(3.0f)); // 0 -> 1 -> 3
    CHECK(f.first_error().empty());
}

TEST_CASE("enums are names, Vec2 is {x, y}, entities are entities, bools and strings work") {
    Fixture f;
    const Entity other = f.world.create(Name{"other"});
    const Entity e = f.world.create(Mover{});
    REQUIRE(f.host->load("fields.luau", R"(
        astral.system { query = {"Mover"}, update = function(ctx, e, m)
            assert(m.mode == "Idle")
            m.mode = "Run"
            m.size = {x = 3, y = 4}
            assert(m.size.x == 3)
            m.active = false
            m.label = "renamed"
            m.target = astral.find("other")
            assert(m.target:get("astral::Name").value == "other")
            m.target = nil
            m.target = astral.find("other")
        end }
    )").empty());
    f.run();
    CHECK(f.first_error().empty());
    const Mover* m = f.world.get<Mover>(e);
    CHECK(m->mode == Mode::Run);
    CHECK(m->size.x == 3.0f);
    CHECK(m->size.y == 4.0f);
    CHECK_FALSE(m->active);
    CHECK(m->label == "renamed");
    CHECK(m->target == other);
}

TEST_CASE("the without filter skips entities; run systems fire once per phase") {
    Fixture f;
    f.world.create(Mover{});
    f.world.create(Mover{}, Frozen{});
    REQUIRE(f.host->load("filter.luau", R"(
        astral.define_event("test::Count")
        local seen = 0
        astral.system { query = {"Mover"}, without = {"Frozen"}, update = function(ctx, e, m) m.steps = m.steps + 1 end }
        astral.system { run = function(ctx) astral.send("test::Count", {n = 1}) end }
    )").empty());
    f.run();
    int steps = 0;
    f.world.each<const Mover>([&](const Mover& m) { steps += m.steps; });
    CHECK(steps == 1);
    CHECK(f.first_error().empty());
}

TEST_CASE("only systems of the called phase run") {
    Fixture f;
    const Entity e = f.world.create(Mover{});
    REQUIRE(f.host->load("phases.luau", R"(
        astral.system { phase = "FixedUpdate", query = {"Mover"}, update = function(ctx, e, m) m.steps = m.steps + 10 end }
        astral.system { phase = "PostUpdate", query = {"Mover"}, update = function(ctx, e, m) m.steps = m.steps + 1 end }
    )").empty());
    f.run(ScriptPhase::Update);
    CHECK(f.world.get<Mover>(e)->steps == 0);
    f.run(ScriptPhase::FixedUpdate);
    f.run(ScriptPhase::PostUpdate);
    CHECK(f.world.get<Mover>(e)->steps == 11);
}

TEST_CASE("script mistakes are contained: syntax errors, runtime errors, endless loops") {
    SUBCASE("a syntax error names the file and line, and loads nothing") {
        Fixture f;
        const auto errors = f.host->load("bad.luau", "local x = 1\nlocal = = 2\n");
        REQUIRE(errors.size() == 1);
        CHECK(contains(errors[0], "bad.luau:2"));
        CHECK_FALSE(f.host->loaded("bad.luau"));
    }
    SUBCASE("a runtime error stops only that module, with file:line") {
        Fixture f;
        const Entity e = f.world.create(Mover{});
        REQUIRE(f.host->load("broken.luau", "astral.system { query = {\"Mover\"}, update = function(ctx, e, m)\n  error(\"boom\")\nend }").empty());
        REQUIRE(f.host->load("fine.luau", "astral.system { query = {\"Mover\"}, update = function(ctx, e, m) m.steps = m.steps + 1 end }").empty());
        f.run();
        f.run();
        const std::string error = f.first_error();
        CHECK(contains(error, "broken.luau:2"));
        CHECK(contains(error, "boom"));
        CHECK(f.host->failed("broken.luau"));
        CHECK_FALSE(f.host->failed("fine.luau"));
        CHECK(f.world.get<Mover>(e)->steps == 2); // the good module kept running
    }
    SUBCASE("an endless loop is cut by the time budget") {
        Fixture f(20.0);
        f.world.create(Mover{});
        REQUIRE(f.host->load("loop.luau", "astral.system { query = {\"Mover\"}, update = function() while true do end end }").empty());
        f.run();
        CHECK(contains(f.first_error(), "time budget"));
        CHECK(f.host->failed("loop.luau"));
    }
    SUBCASE("unknown names say what exists") {
        Fixture f;
        f.world.create(Mover{});
        const auto errors = f.host->load("typo.luau", "astral.system { query = {\"Movr\"}, update = function() end }");
        REQUIRE(errors.size() == 1);
        CHECK(contains(errors[0], "unknown component \"Movr\""));
        CHECK(contains(errors[0], "Mover")); // the known list
        REQUIRE(f.host->load("field.luau", "astral.system { query = {\"Mover\"}, update = function(c, e, m) m.spede = 1 end }").empty());
        f.run();
        const std::string error = f.first_error();
        CHECK(contains(error, "no field \"spede\""));
        CHECK(contains(error, "speed"));
    }
    SUBCASE("a wrong value type names the field") {
        Fixture f;
        f.world.create(Mover{});
        REQUIRE(f.host->load("type.luau", "astral.system { query = {\"Mover\"}, update = function(c, e, m) m.steps = \"many\" end }").empty());
        f.run();
        const std::string error = f.first_error();
        CHECK(contains(error, "steps"));
        CHECK(contains(error, "number"));
    }
    SUBCASE("a script cannot reach the operating system") {
        Fixture f;
        REQUIRE(f.host->load("os.luau", "astral.system { run = function() os.execute('x') end }").empty());
        f.run();
        CHECK_FALSE(f.first_error().empty());
        Fixture g;
        CHECK_FALSE(g.host->load("io.luau", "local f = io.open('x')").empty());
    }
}

TEST_CASE("reloading replaces the module; a broken reload keeps the old one running") {
    Fixture f;
    const Entity e = f.world.create(Mover{});
    REQUIRE(f.host->load("rule.luau", "astral.system { query = {\"Mover\"}, update = function(c, e, m) m.steps = m.steps + 1 end }").empty());
    f.run();
    CHECK(f.world.get<Mover>(e)->steps == 1);
    CHECK_FALSE(f.host->load("rule.luau", "this is not lua").empty());
    f.run();
    CHECK(f.world.get<Mover>(e)->steps == 2); // the old version still runs
    REQUIRE(f.host->load("rule.luau", "astral.system { query = {\"Mover\"}, update = function(c, e, m) m.steps = m.steps + 100 end }").empty());
    f.run();
    CHECK(f.world.get<Mover>(e)->steps == 102); // the new one replaced it (state in the component survived)
    CHECK(f.host->modules().size() == 1);
    f.host->unload("rule.luau");
    f.run();
    CHECK(f.world.get<Mover>(e)->steps == 102);
    // A failed module recovers when its file is fixed.
    REQUIRE(f.host->load("flaky.luau", "astral.system { run = function() error('x') end }").empty());
    f.run();
    CHECK(f.host->failed("flaky.luau"));
    REQUIRE(f.host->load("flaky.luau", "astral.system { run = function() end }").empty());
    CHECK_FALSE(f.host->failed("flaky.luau"));
    (void)f.host->take_errors();
}

TEST_CASE("script-defined components are real components: scripts, scenes and saves see them") {
    Fixture f;
    REQUIRE(f.host->load("health.luau", R"(
        astral.define_component("game::Health", {
            value = {type = "f32", default = 100},
            max = "f32",
            facing = {type = "vec3", default = {x = 0, y = 0, z = 1}},
        })
        astral.system { run = function()
            local hero = astral.spawn { ["astral::Name"] = {value = "hero"}, ["game::Health"] = {max = 150} }
            local h = hero:get("game::Health")
            assert(h.value == 100 and h.max == 150)
            assert(h.facing.z == 1)
            h.value = h.value - 30
            assert(hero:has("game::Health"))
        end }
    )").empty());
    f.run();
    CHECK(f.first_error().empty());
    const Entity hero = find_by_name(f.world, "hero");
    REQUIRE(hero.valid());
    const auto* health = f.types.find("game::Health");
    REQUIRE(health);
    CHECK(component_to_json(f.world, *health, hero)["value"].as_number() == 70.0);
    // Defining the same schema again (a reload) is fine; a different one is refused.
    CHECK(f.host->load("health.luau", "astral.define_component('game::Health', {value = 'f32', max = 'f32', facing = 'vec3'})").empty());
    CHECK_FALSE(f.host->load("other.luau", "astral.define_component('game::Health', {value = 'i32'})").empty());
}

TEST_CASE("events: C++ events reach listeners as tables, scripts send events back, dynamic events work") {
    Fixture f;
    const Entity who = f.world.create(Name{"someone"});
    REQUIRE(f.host->load("events.luau", R"(
        astral.define_event("game::Seen")
        astral.on("Pinged", function(ctx, ev)
            assert(ev.who:get("astral::Name").value == "someone")
            astral.send("game::Seen", {strength = ev.strength, x = ev.where.x})
            astral.send("Pinged", {strength = ev.strength + 1, where = vector.create(1, 2, 3)})
        end)
        astral.on("game::Seen", function(ctx, ev) astral.log("seen " .. ev.strength) end)
    )").empty());
    f.world.send(Pinged{2.0f, {5, 0, 0}, who});
    f.run();
    CHECK(f.first_error().empty());
    // The script's reply is a typed C++ event now.
    EventReader<Pinged> reader;
    std::vector<Pinged> got;
    reader.read(f.world, [&](const Pinged& p) { got.push_back(p); });
    REQUIRE(got.size() == 2); // the original and the reply
    CHECK(got[1].strength == doctest::Approx(3.0f));
    CHECK(got[1].where.z == doctest::Approx(3.0f));
    CHECK(contains(f.host->modules().front(), "events"));
}

TEST_CASE("a behaviour runs for entities with a Script component: on_start once, on_update every frame") {
    Fixture f;
    const Entity door = f.world.create(Mover{}, Script{"door.luau"});
    REQUIRE(f.host->load("door.luau", R"(
        astral.behaviour {
            on_start = function(self, ctx) self:get("Mover").steps = 100 end,
            on_update = function(self, ctx) local m = self:get("Mover"); m.steps = m.steps + 1 end,
            on_fixed_update = function(self, ctx) local m = self:get("Mover"); m.speed = m.speed + 1 end,
        }
    )").empty());
    f.run();
    f.run();
    f.run(ScriptPhase::FixedUpdate);
    CHECK(f.world.get<Mover>(door)->steps == 102);
    CHECK(f.world.get<Mover>(door)->speed == doctest::Approx(3.0f));
    f.world.destroy(door);
    f.run(); // a dead entity is simply gone
    CHECK(f.first_error().empty());
}

TEST_CASE("timers fire once or repeat, and can be cancelled") {
    Fixture f;
    const Entity e = f.world.create(Mover{});
    REQUIRE(f.host->load("timers.luau", R"(
        local function bump(n) local m = astral.find("t"):get("Mover"); m.steps = m.steps + n end
        astral.system { run = function(ctx)
            if ctx.time == 0 then
                astral.after(1.0, function() bump(100) end)
                astral.every(0.5, function() bump(1) end)
                local id = astral.after(0.5, function() bump(1000) end)
                astral.cancel(id)
            end
        end }
    )").empty());
    f.world.add(e, Name{"t"});
    f.host->run(ScriptPhase::Update, {.delta = 0.25f, .elapsed = 0.0}); // arms the timers, 0.25 s pass
    for (int i = 0; i < 7; ++i) f.host->run(ScriptPhase::Update, {.delta = 0.25f, .elapsed = 1.0 + i});
    // 2 s total: every(0.5) fired 4 times, after(1.0) once, the cancelled one never.
    CHECK(f.world.get<Mover>(e)->steps == 104);
    CHECK(f.first_error().empty());
}

TEST_CASE("random is seeded: the same seed gives the same numbers") {
    Fixture a, b;
    const char* code = R"(
        astral.define_component("t::Rolls", {a = "f32", b = "f32"})
        astral.system { run = function()
            astral.seed(42)
            local e = astral.spawn { ["t::Rolls"] = {a = astral.random(), b = astral.random()} }
        end }
    )";
    REQUIRE(a.host->load("r.luau", code).empty());
    REQUIRE(b.host->load("r.luau", code).empty());
    a.run();
    b.run();
    const auto roll = [](Fixture& f) {
        const auto* t = f.types.find("t::Rolls");
        double v = -1;
        f.world.for_each_entity([&](Entity e) {
            if (t->has(f.world, e)) v = component_to_json(f.world, *t, e)["a"].as_number() * 1000 + component_to_json(f.world, *t, e)["b"].as_number();
        });
        return v;
    };
    CHECK(roll(a) == roll(b));
    CHECK(roll(a) > 0);
}

TEST_CASE("entities that died are reported, not crashed on") {
    Fixture f;
    f.world.create(Name{"victim"});
    REQUIRE(f.host->load("dead.luau", R"(
        astral.system { run = function()
            local e = astral.find("victim")
            e:destroy()
            assert(not e:alive())
            local ok, err = pcall(function() return e:get("astral::Name") end)
            assert(not ok)
        end }
    )").empty());
    f.run();
    CHECK(f.first_error().empty());
}

TEST_CASE("astral.query lists entities with live component proxies") {
    Fixture f;
    f.world.create(Mover{}, Name{"a"});
    f.world.create(Mover{});
    f.world.create(Name{"c"});
    REQUIRE(f.host->load("q.luau", R"(
        astral.system { run = function()
            local rows = astral.query({"Mover", "astral::Name"})
            assert(#rows == 1)
            local entity, mover, name = rows[1][1], rows[1][2], rows[1][3]
            assert(name.value == "a")
            mover.steps = 7
            assert(#astral.query({"Mover"}) == 2)
            assert(#astral.query({}) >= 3)
        end }
    )").empty());
    f.run();
    CHECK(f.first_error().empty());
    CHECK(f.world.get<Mover>(find_by_name(f.world, "a"))->steps == 7);
}

TEST_CASE("astral.quat composes and rotates like the C++ Quat") {
    Fixture f;
    const Entity e = f.world.create(Transform{});
    REQUIRE(f.host->load("quat.luau", R"(
        astral.system { query = {"astral::Transform"}, update = function(ctx, e, t)
            local yaw = astral.quat.axis_angle(vector.create(0, 1, 0), math.pi / 2)
            local pitch = astral.quat.axis_angle(vector.create(1, 0, 0), 0.3)
            t.rotation = astral.quat.normalize(astral.quat.mul(yaw, pitch))
            local forward = astral.quat.rotate(yaw, vector.create(0, 0, -1))
            t.position = forward
            assert(not pcall(astral.quat.rotate, {x = 0}, vector.create(1, 0, 0)))
        end }
    )").empty());
    f.run();
    CHECK(f.first_error().empty());
    const Transform* t = f.world.get<Transform>(e);
    const Quat expected = normalize(Quat::from_axis_angle({0, 1, 0}, 1.5707964f) * Quat::from_axis_angle({1, 0, 0}, 0.3f));
    CHECK(t->rotation.x == doctest::Approx(expected.x).epsilon(1e-4));
    CHECK(t->rotation.y == doctest::Approx(expected.y).epsilon(1e-4));
    CHECK(t->rotation.w == doctest::Approx(expected.w).epsilon(1e-4));
    CHECK(t->position.x == doctest::Approx(-1.0f).epsilon(1e-4)); // -z turned by +90 degrees about y
}
