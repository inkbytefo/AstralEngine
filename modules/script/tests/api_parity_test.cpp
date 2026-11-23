// Parity tests between runtime Luau bindings, ScriptApiCatalog, generated API.md / astral.d.luau,
// and TypeRegistry / EventRegistry (Inspector discovery surface) — ADR-0034.
#include <astral/script/api.hpp>
#include <astral/script/script.hpp>
#include <astral/world/scene.hpp>
#include <doctest/doctest.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace astral;

namespace {

struct ParityStats {
    float speed = 5.0f;
    Entity target{};
};

struct ParityHit {
    Entity actor{};
    int damage = 1;
};

struct MemoryStorage final : ScriptStorage {
    std::unordered_map<std::string, std::string> slots;
    bool write(std::string_view slot, std::string_view text) override {
        slots[std::string(slot)] = std::string(text);
        return true;
    }
    std::optional<std::string> read(std::string_view slot) const override {
        auto it = slots.find(std::string(slot));
        return it == slots.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
};

} // namespace

ASTRAL_REFLECT(ParityStats, speed, target)
ASTRAL_REFLECT(ParityHit, actor, damage)

TEST_CASE("runtime Luau astral bindings match ScriptApiCatalog and generated astral.d.luau / API.md") {
    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    register_world_types(types);
    types.add<ParityStats>();
    types.set_schema_version<ParityStats>(2);
    events.add<ParityHit>();

    const std::map<std::string, std::string> owners{
        {"ParityStats", "combat"},
        {"ParityHit", "combat"},
    };
    const ApiDiscoverySummary discovery = collect_api_discovery(types, events, owners);
    const ScriptApi generated = generate_script_api(types, events, owners);

    // Every Inspector-visible TypeRegistry entry appears in discovery, astral.d.luau, and API.md with its fields.
    REQUIRE(discovery.components.size() == types.entries().size());
    for (const TypeRegistry::Entry& entry : types.entries()) {
        const std::string name(entry.name);
        CHECK(generated.luau_types.find("[\"" + name + "\"]") != std::string::npos);
        CHECK(generated.markdown.find("### `" + name + "`") != std::string::npos);
        for (const FieldInfo& f : entry.fields) {
            if (f.kind != FieldKind::Other) {
                CHECK(generated.luau_types.find(f.name + ":") != std::string::npos);
                CHECK(generated.markdown.find("`" + f.name + "`") != std::string::npos);
            }
        }
    }
    CHECK(generated.markdown.find("(schema v2)") != std::string::npos);
    CHECK(generated.markdown.find("(feature `combat`)") != std::string::npos);

    // Every symbol in ScriptApiCatalog exists in the live Luau VM (exact 1:1 parity on astral and subtables).
    MemoryStorage storage;
    ScriptHost host({world, types, events, log, nullptr, &storage});
    std::vector<std::string> runtime_keys;
    const auto errors = host.load("parity.luau", R"(
        local function collect(prefix, tbl)
            for k, v in pairs(tbl) do
                if type(v) == "function" then
                    astral.send("ParityReport", { key = prefix .. k })
                elseif type(v) == "table" then
                    collect(prefix .. k .. ".", v)
                end
            end
        end
        astral.define_event("ParityReport")
        astral.system {
            phase = "Update",
            run = function()
                collect("", astral)
            end
        }
    )");
    REQUIRE(errors.empty());
    host.run(ScriptPhase::Update, {.delta = 1.0f / 60.0f});
    CHECK(host.take_errors().empty());

    const EventRegistry::Entry* report = events.find("ParityReport");
    REQUIRE(report != nullptr);
    std::uint64_t cursor = 0;
    std::set<std::string> live_symbols;
    report->read(world, cursor, [&](const Json& payload) {
        live_symbols.insert(payload["key"].as_string());
    });

    std::set<std::string> catalog_symbols;
    for (const ScriptSymbolDesc& sym : script_api_catalog().symbols) {
        if (sym.table == "Entity") {
            CHECK(generated.luau_types.find("    " + std::string(sym.name) + ": ") != std::string::npos);
            continue;
        }
        const std::string key = sym.table.empty() ? std::string(sym.name) : std::string(sym.table) + "." + std::string(sym.name);
        catalog_symbols.insert(key);
        CHECK(generated.luau_types.find(std::string(sym.name) + ": ") != std::string::npos);
    }
    CHECK(live_symbols == catalog_symbols);
}

TEST_CASE("astral.save and astral.load rebind live entity handles and null dangling references") {
    World world;
    TypeRegistry types;
    EventRegistry events;
    Logger log;
    register_world_types(types);
    types.add<ParityStats>();
    MemoryStorage storage;

    const Entity outsider = world.create(); // no saved components -> excluded from slot1
    const Entity b = world.create(Name{"b"}, ParityStats{.speed = 2.0f, .target = outsider});
    const Entity a = world.create(Name{"a"}, ParityStats{.speed = 9.0f, .target = b});

    ScriptHost host({world, types, events, log, nullptr, &storage});
    REQUIRE(host.load("save_rebind.luau", R"(
        local step = 0
        astral.system {
            phase = "Update",
            run = function()
                step += 1
                if step == 1 then
                    assert(astral.save("slot1", {"astral::Name", "ParityStats"}))
                elseif step == 2 then
                    assert(astral.load("slot1"))
                end
            end
        }
    )").empty());

    host.run(ScriptPhase::Update, {.delta = 1.0f / 60.0f}); // save
    world.destroy(outsider);
    host.run(ScriptPhase::Update, {.delta = 1.0f / 60.0f}); // load
    CHECK(host.take_errors().empty());
    CHECK_FALSE(world.alive(a));
    CHECK_FALSE(world.alive(b));

    const Entity a2 = find_by_name(world, "a");
    const Entity b2 = find_by_name(world, "b");
    REQUIRE(a2.valid());
    REQUIRE(b2.valid());
    CHECK(world.get<const ParityStats>(a2)->target == b2);
    CHECK(world.get<const ParityStats>(a2)->speed == doctest::Approx(9.0f));
    CHECK_FALSE(world.get<const ParityStats>(b2)->target.valid());
}
