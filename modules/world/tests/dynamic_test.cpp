// Raw component access, field tables, script-defined components, named events (M11.1).
#include <astral/world/event_registry.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/serialize.hpp>

#include <doctest/doctest.h>

namespace {
enum class Team : std::uint8_t { Red, Blue };
struct Stats {
    float speed = 3.0f;
    std::int32_t lives = 2;
    bool alive = true;
    astral::Vec3 aim{0, 1, 0};
    Team team = Team::Blue;
    std::string label = "x";
    astral::Entity target{};
};
struct Hit {
    float damage = 0;
    astral::Entity victim{};
};
} // namespace
ASTRAL_REFLECT_ENUM(Team, Red, Blue)
ASTRAL_REFLECT(Stats, speed, lives, alive, aim, team, label, target)
ASTRAL_REFLECT(Hit, damage, victim)

using namespace astral;

TEST_CASE("raw access reads and writes any component by id, and marks writes as changes") {
    World w;
    const Entity e = w.create(Stats{});
    const ComponentId id = component_id<Stats>();
    CHECK(w.has_raw(e, id));
    CHECK_FALSE(w.has_raw(e, component_id<Hit>()));
    w.clear_trackers();
    const Stats* before = reinterpret_cast<const Stats*>(static_cast<const World&>(w).get_raw(e, id));
    REQUIRE(before);
    CHECK(before->lives == 2);
    CHECK(w.count<Stats>() == 1);
    CHECK(w.query<Stats>().changed<Stats>().count() == 0); // reading did not mark it
    auto* mutable_stats = reinterpret_cast<Stats*>(w.get_raw(e, id));
    mutable_stats->lives = 5;
    CHECK(w.query<Stats>().changed<Stats>().count() == 1);
    CHECK(w.get<Stats>(e)->lives == 5);

    Hit hit{7.0f, e};
    CHECK(w.add_raw(e, component_info<Hit>(), &hit));
    CHECK(w.get<Hit>(e)->damage == 7.0f);
    CHECK(w.remove_raw(e, component_id<Hit>()));
    CHECK_FALSE(w.has<Hit>(e));
    CHECK(w.get_raw(Entity{999, 1}, id) == nullptr); // dead entity: null, no crash
}

TEST_CASE("each_raw visits the entities that have every listed component") {
    World w;
    const Entity a = w.create(Stats{}, Hit{});
    w.create(Stats{});
    std::vector<Entity> seen;
    const std::vector<ComponentId> ids{component_id<Stats>(), component_id<Hit>()};
    w.each_raw(ids, [&](Entity e) { seen.push_back(e); });
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == a);
}

TEST_CASE("registered types expose a field table: names, kinds, offsets, enumerators") {
    TypeRegistry types;
    types.add<Stats>();
    const auto* t = types.find("Stats");
    REQUIRE(t);
    REQUIRE(t->fields.size() == 7);
    CHECK(t->fields[0].name == "speed");
    CHECK(t->fields[0].kind == FieldKind::Float);
    CHECK(t->fields[1].kind == FieldKind::Int);
    CHECK(t->fields[2].kind == FieldKind::Bool);
    CHECK(t->fields[3].kind == FieldKind::Vec3);
    CHECK(t->fields[4].kind == FieldKind::Enum);
    CHECK(t->fields[4].enumerators.size() == 2);
    CHECK(t->fields[4].enumerators[1].first == "Blue");
    CHECK(t->fields[5].kind == FieldKind::String);
    CHECK(t->fields[6].kind == FieldKind::EntityRef);
    Stats s;
    CHECK(t->fields[1].offset == offsetof(Stats, lives));
    CHECK(*reinterpret_cast<const std::int32_t*>(reinterpret_cast<const char*>(&s) + t->fields[1].offset) == 2);
}

TEST_CASE("a script-defined component lives in the ECS, saves and loads like a C++ one") {
    TypeRegistry types;
    register_world_types(types);
    std::string error;
    const TypeRegistry::Entry* health = types.add_dynamic(
        "game::Health", {{"value", FieldKind::Float, 100.0}, {"max", FieldKind::Float, 100.0}, {"armor", FieldKind::Int, 0.0}, {"dead", FieldKind::Bool, false}},
        &error);
    REQUIRE_MESSAGE(health, error);
    CHECK(types.add_dynamic("game::Health", {{"x", FieldKind::Float, 0.0}}, &error) == nullptr); // redefinition refused
    CHECK_FALSE(error.empty());
    CHECK(types.add_dynamic("bad::Text", {{"s", FieldKind::String, ""}}, &error) == nullptr); // strings are not plain data

    World w;
    const Entity e = w.create(Name{"hero"});
    LoadContext ctx;
    component_from_json(w, *health, e, Json::parse(R"({"value": 40, "armor": 3})").value());
    REQUIRE(health->has(w, e));
    const auto* raw = static_cast<const std::byte*>(static_cast<const World&>(w).get_raw(e, health->component));
    float value = 0, max = 0;
    std::int32_t armor = 0;
    std::memcpy(&value, raw + health->fields[0].offset, sizeof value);
    std::memcpy(&max, raw + health->fields[1].offset, sizeof max);
    std::memcpy(&armor, raw + health->fields[2].offset, sizeof armor);
    CHECK(value == 40.0f);
    CHECK(max == 100.0f); // default from the definition
    CHECK(armor == 3);

    const Json doc = save_world(w, types);
    World other;
    CHECK(load_world(other, types, doc).errors.empty());
    const Entity copy = find_by_name(other, "hero");
    REQUIRE(copy.valid());
    CHECK(health->has(other, copy));
    CHECK(component_to_json(other, *health, copy)["value"].as_number() == 40.0);
}

TEST_CASE("events are sent and read by name, reflected C++ events and dynamic ones alike") {
    EventRegistry events;
    events.add<Hit>();
    events.add_dynamic("game::Ping");
    World w;
    std::uint64_t hit_cursor = 0, ping_cursor = 0;
    const auto* hit = events.find("Hit");
    const auto* ping = events.find("game::Ping");
    REQUIRE(hit);
    REQUIRE(ping);
    CHECK(events.find("nope") == nullptr);

    const Entity e = w.create();
    w.send(Hit{3.0f, e});
    ping->send(w, Json::parse(R"({"n": 2})").value(), nullptr);
    ping->send(w, Json::parse(R"({"n": 3})").value(), nullptr);
    std::vector<double> damage, pings;
    hit->read(w, hit_cursor, [&](const Json& j) { damage.push_back(j["damage"].as_number()); });
    ping->read(w, ping_cursor, [&](const Json& j) { pings.push_back(j["n"].as_number()); });
    CHECK(damage == std::vector<double>{3.0});
    CHECK(pings == std::vector<double>{2.0, 3.0});
    hit->send(w, Json::parse(R"({"damage": 9})").value(), nullptr); // JSON -> typed event
    damage.clear();
    hit->read(w, hit_cursor, [&](const Json& j) { damage.push_back(j["damage"].as_number()); });
    CHECK(damage == std::vector<double>{9.0});
    pings.clear();
    ping->read(w, ping_cursor, [&](const Json& j) { pings.push_back(j["n"].as_number()); });
    CHECK(pings.empty()); // already read
}
