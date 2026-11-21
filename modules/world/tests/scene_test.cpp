#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <doctest/doctest.h>

#include <map>
#include <string>

namespace scene_test {
enum class Kind : std::uint8_t { Small, Large = 7, Huge };
struct Light {
    float intensity = 1.0f;
    astral::Vec3 color{1, 1, 1};
    Kind kind = Kind::Small;
};
struct Target {
    astral::Entity entity;
};
} // namespace scene_test

ASTRAL_REFLECT_ENUM(scene_test::Kind, Small, Large, Huge)
ASTRAL_REFLECT(scene_test::Light, intensity, color, kind)
ASTRAL_REFLECT(scene_test::Target, entity)

using namespace astral;
using scene_test::Light;
using scene_test::Target;

namespace {

/// Documents in memory; `set` bumps the version like a file reload would.
struct Docs {
    std::map<std::string, std::pair<Json, std::uint32_t>> files;
    void set(const std::string& path, const char* text) {
        std::string error;
        auto j = Json::parse(text, &error);
        REQUIRE_MESSAGE(j, error);
        auto& f = files[path];
        f.first = *j;
        ++f.second;
    }
    DocumentSource source() {
        return [this](std::string_view path) {
            auto it = files.find(std::string(path));
            if (it == files.end()) return SourceDocument{.error = "no such file"};
            return SourceDocument{.json = &it->second.first, .version = it->second.second, .error = {}};
        };
    }
};

TypeRegistry make_types() {
    TypeRegistry t;
    register_world_types(t);
    t.add<Light>();
    t.add<Target>();
    return t;
}

Json parse(const char* text) {
    std::string error;
    auto j = Json::parse(text, &error);
    REQUIRE_MESSAGE(j, error);
    return *j;
}

const char* lamp_prefab = R"({"format": "astral.scene", "version": 1, "entities": [
  {"id": 1, "components": {"astral::Name": {"value": "lamp"}, "astral::Transform": {}}},
  {"id": 2, "components": {"astral::Name": {"value": "bulb"}, "astral::Parent": {"entity": 1},
                            "astral::Transform": {"position": [0, 2, 0]},
                            "scene_test::Light": {"intensity": 10, "color": [1, 0.5, 0.25]}}},
  {"id": 3, "components": {"astral::Name": {"value": "shade"}, "astral::Parent": {"entity": 2},
                            "astral::Transform": {}}}]})";

Entity part(World& w, Entity instance, const char* name) {
    const Entity e = find_part(w, instance, name);
    REQUIRE(e.valid());
    return e;
}

} // namespace

TEST_CASE("enums are saved by name and read by name or number") {
    World w;
    TypeRegistry types = make_types();
    const Entity e = w.create(Light{.kind = scene_test::Kind::Huge});
    Json doc = save_world(w, types);
    CHECK(doc["entities"][0]["components"]["scene_test::Light"]["kind"].as_string() == "Huge");

    World w2;
    Json in = parse(R"({"format": "astral.world", "version": 1, "entities": [
        {"id": 0, "components": {"scene_test::Light": {"kind": "Large"}}},
        {"id": 1, "components": {"scene_test::Light": {"kind": 8}}},
        {"id": 2, "components": {"scene_test::Light": {"kind": "Medium"}}}]})");
    LoadResult r = load_world(w2, types, in);
    CHECK(w2.get<Light>(r.entities[0])->kind == scene_test::Kind::Large);
    CHECK(w2.get<Light>(r.entities[1])->kind == scene_test::Kind::Huge);
    CHECK(w2.get<Light>(r.entities[2])->kind == scene_test::Kind::Small); // unknown name: default kept
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].find("Medium") != std::string::npos);
    (void)e;
}

TEST_CASE("a scene keeps its record ids across saves") {
    Docs docs;
    TypeRegistry types = make_types();
    PrefabLibrary prefabs(types, docs.source());
    World w;
    LoadResult r = load_scene(w, prefabs, parse(R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 4, "components": {"astral::Name": {"value": "a"}, "astral::Transform": {}}},
        {"id": 9, "components": {"astral::Name": {"value": "b"}, "astral::Parent": {"entity": 4}}},
        {"id": 12, "components": {"scene_test::Target": {"entity": 9}}}]})"));
    REQUIRE(r.errors.empty());
    REQUIRE(r.entities.size() == 3);
    CHECK(w.get<Parent>(r.entities[1])->entity == r.entities[0]);
    CHECK(w.get<Target>(r.entities[2])->entity == r.entities[1]);
    CHECK(w.get<SceneId>(r.entities[1])->value == 9);

    w.destroy(r.entities[0]);
    const Entity added = w.create(Name{"new"});
    const Json saved = save_scene(w, prefabs);
    const Json& list = saved["entities"];
    REQUIRE(list.size() == 3);
    CHECK(list[0]["id"].as_number() == 9);
    CHECK(list[0]["components"]["astral::Parent"]["entity"].is_null()); // its parent is gone
    CHECK(list[1]["id"].as_number() == 12);
    CHECK(list[1]["components"]["scene_test::Target"]["entity"].as_number() == 9);
    CHECK(list[2]["id"].as_number() == 13); // next free id, kept on the entity
    CHECK(w.get<SceneId>(added)->value == 13);
    CHECK(save_scene(w, prefabs) == saved); // saving again changes nothing
}

TEST_CASE("an instance loads with its overrides and saves only what differs") {
    Docs docs;
    docs.set("lamp.prefab", lamp_prefab);
    TypeRegistry types = make_types();
    PrefabLibrary prefabs(types, docs.source());
    const Json scene = parse(R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "prefab": "lamp.prefab",
         "components": {"astral::Transform": {"position": [5, 0, 0]}},
         "parts": {"2": {"components": {"scene_test::Light": {"intensity": 40}}}}}]})");
    World w;
    LoadResult r = load_scene(w, prefabs, scene);
    REQUIRE(r.errors.empty());
    const Entity lamp = r.entities[0];
    CHECK(w.get<PrefabInstance>(lamp)->path == "lamp.prefab");
    CHECK(w.get<Name>(lamp)->value == "lamp");
    CHECK(w.get<Transform>(lamp)->position.x == 5.0f);
    const Entity bulb = part(w, lamp, "bulb");
    const Entity shade = part(w, lamp, "shade");
    CHECK(w.get<PrefabPart>(bulb)->path == "2");
    CHECK(w.get<Parent>(bulb)->entity == lamp);
    CHECK(w.get<Parent>(shade)->entity == bulb);
    CHECK(w.get<Light>(bulb)->intensity == 40.0f);    // overridden
    CHECK(w.get<Light>(bulb)->color.y == 0.5f);       // from the prefab
    CHECK(w.get<Transform>(bulb)->position.y == 2.0f);

    CHECK(save_scene(w, prefabs) == scene); // nothing changed: the same minimal record

    SUBCASE("edits become overrides") {
        w.get<Light>(bulb)->color.z = 1.0f;
        w.remove<Transform>(lamp);
        w.add(shade, Light{.intensity = 3});
        const Json saved = save_scene(w, prefabs);
        const Json& rec = saved["entities"][0];
        CHECK(rec["components"].size() == 0);
        CHECK(rec["removed"][0].as_string() == "astral::Transform");
        const Json& bulb_patch = rec["parts"]["2"]["components"]["scene_test::Light"];
        CHECK(bulb_patch.size() == 2); // intensity (from the scene) and color, not every field
        CHECK(bulb_patch["intensity"].as_number() == 40);
        CHECK(bulb_patch["color"][2].as_number() == 1);
        CHECK(rec["parts"]["3"]["components"]["scene_test::Light"]["intensity"].as_number() == 3);

        World w2;
        REQUIRE(load_scene(w2, prefabs, saved).errors.empty());
        const Entity lamp2 = find_by_name(w2, "lamp");
        CHECK_FALSE(w2.has<Transform>(lamp2));
        CHECK(w2.get<Light>(find_part(w2, lamp2, "bulb"))->color.z == 1.0f);
        CHECK(w2.get<Light>(find_part(w2, lamp2, "shade"))->intensity == 3.0f);
        CHECK(save_scene(w2, prefabs) == saved);
    }

    SUBCASE("destroyed parts are recorded as deleted, with their children") {
        w.destroy(bulb);
        w.destroy(shade);
        const Json saved = save_scene(w, prefabs);
        const Json& deleted = saved["entities"][0]["deleted"];
        REQUIRE(deleted.size() == 1); // the shade goes with the bulb
        CHECK(deleted[0].as_string() == "2");
        World w2;
        REQUIRE(load_scene(w2, prefabs, saved).errors.empty());
        CHECK(w2.query<const PrefabPart>().count() == 0);
    }

    SUBCASE("parts are destroyed with their instance") {
        w.destroy(lamp);
        sync_prefabs(w, prefabs);
        CHECK(w.query<const PrefabPart>().count() == 0);
    }
}

TEST_CASE("references into instances, nested prefabs and variants") {
    Docs docs;
    docs.set("lamp.prefab", lamp_prefab);
    // A desk with a lamp on it; the lamp's bulb is dimmer here.
    docs.set("desk.prefab", R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "components": {"astral::Name": {"value": "desk"}, "astral::Transform": {}}},
        {"id": 5, "prefab": "lamp.prefab",
         "components": {"astral::Parent": {"entity": 1}, "astral::Transform": {"position": [0, 1, 0]}},
         "parts": {"2": {"components": {"scene_test::Light": {"intensity": 4}}}}},
        {"id": 6, "components": {"astral::Name": {"value": "note"}, "astral::Parent": {"entity": 1}, "scene_test::Target": {"entity": "5/2"}}}]})");
    // A variant: the desk, red light, no shade.
    docs.set("red_desk.prefab", R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "prefab": "desk.prefab",
         "parts": {"5/2": {"components": {"scene_test::Light": {"color": [1, 0, 0]}}}},
         "deleted": ["5/3"]}]})");
    TypeRegistry types = make_types();
    PrefabLibrary prefabs(types, docs.source());

    std::string error;
    auto tpl = prefabs.get("red_desk.prefab", &error);
    REQUIRE_MESSAGE(tpl, error);
    CHECK(tpl->parts[0].path.empty());
    CHECK(tpl->parts.size() == 4); // desk, lamp root, bulb, note (shade deleted)

    const Json scene = parse(R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "prefab": "red_desk.prefab"},
        {"id": 2, "components": {"astral::Name": {"value": "fly"}, "astral::Parent": {"entity": "1/5/2"}}}]})");
    World w;
    LoadResult r = load_scene(w, prefabs, scene);
    REQUIRE_MESSAGE(r.errors.empty(), r.errors[0]);
    const Entity desk = r.entities[0];
    CHECK(w.get<Name>(desk)->value == "desk");
    const Entity lamp = part(w, desk, "lamp");
    const Entity bulb = part(w, desk, "bulb");
    CHECK(w.get<PrefabPart>(lamp)->path == "5");
    CHECK(w.get<PrefabPart>(bulb)->path == "5/2");
    CHECK(w.get<Parent>(lamp)->entity == desk);
    CHECK(w.get<Parent>(bulb)->entity == lamp);
    CHECK(w.get<Light>(bulb)->intensity == 4.0f); // desk's override
    CHECK(w.get<Light>(bulb)->color.x == 1.0f);   // variant's override
    CHECK(w.get<Light>(bulb)->color.y == 0.0f);
    CHECK_FALSE(find_part(w, desk, "shade").valid());
    CHECK(w.get<Target>(part(w, desk, "note"))->entity == bulb);
    CHECK(w.get<Parent>(find_by_name(w, "fly"))->entity == bulb);
    CHECK(save_scene(w, prefabs) == scene);
}

TEST_CASE("prefab errors") {
    Docs docs;
    docs.set("a.prefab", R"({"format": "astral.scene", "version": 1, "entities": [{"id": 1, "prefab": "b.prefab"}]})");
    docs.set("b.prefab", R"({"format": "astral.scene", "version": 1, "entities": [{"id": 1, "prefab": "a.prefab"}]})");
    docs.set("two_roots.prefab", R"({"format": "astral.scene", "version": 1, "entities": [{"id": 1}, {"id": 2}]})");
    TypeRegistry types = make_types();
    PrefabLibrary prefabs(types, docs.source());
    std::string error;
    CHECK_FALSE(prefabs.get("a.prefab", &error));
    CHECK(error.find("includes itself") != std::string::npos);
    CHECK_FALSE(prefabs.get("two_roots.prefab", &error));
    CHECK(error.find("one root") != std::string::npos);
    CHECK_FALSE(prefabs.get("missing.prefab", &error));

    World w;
    LoadResult r = load_scene(w, prefabs, parse(R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "prefab": "missing.prefab", "components": {"astral::Transform": {"position": [1, 2, 3]}}},
        {"id": 1, "components": {}}]})"));
    CHECK(r.errors.size() == 2); // missing prefab, duplicate id
    REQUIRE(r.entities.size() == 1);
    CHECK(w.get<Transform>(r.entities[0])->position.y == 2.0f); // kept: spawns once the prefab exists
    const Json saved = save_scene(w, prefabs);
    CHECK(saved["entities"][0]["prefab"].as_string() == "missing.prefab");
    CHECK(saved["entities"][0]["components"]["astral::Transform"]["position"][2].as_number() == 3);

    CHECK_FALSE(load_scene(w, prefabs, parse(R"({"format": "astral.world", "version": 1})")).errors.empty());
}

TEST_CASE("instances created by code spawn in sync_prefabs; a reloaded prefab keeps overrides") {
    Docs docs;
    docs.set("lamp.prefab", lamp_prefab);
    TypeRegistry types = make_types();
    PrefabLibrary prefabs(types, docs.source());
    World w;
    const Entity target = w.create(Name{"target"});
    const Entity lamp = w.create(Transform{.position = {0, 0, 9}}, PrefabInstance{"lamp.prefab"});
    std::vector<std::string> errors;
    sync_prefabs(w, prefabs, &errors);
    REQUIRE(errors.empty());
    CHECK(w.get<Transform>(lamp)->position.z == 9.0f); // its own component wins
    CHECK(w.get<Name>(lamp)->value == "lamp");          // the rest comes from the prefab
    Entity bulb = part(w, lamp, "bulb");
    w.get<Light>(bulb)->intensity = 99.0f;
    w.add(part(w, lamp, "shade"), Target{target});
    sync_prefabs(w, prefabs); // nothing changed: nothing respawns
    CHECK(w.alive(bulb));

    // The artist edits the prefab: a new colour, and the bulb moves.
    docs.set("lamp.prefab", R"({"format": "astral.scene", "version": 1, "entities": [
      {"id": 1, "components": {"astral::Name": {"value": "lamp"}, "astral::Transform": {}}},
      {"id": 2, "components": {"astral::Name": {"value": "bulb"}, "astral::Parent": {"entity": 1},
                                "astral::Transform": {"position": [0, 3, 0]},
                                "scene_test::Light": {"intensity": 10, "color": [0, 1, 0]}}},
      {"id": 3, "components": {"astral::Name": {"value": "shade"}, "astral::Parent": {"entity": 2},
                                "astral::Transform": {}}}]})");
    CHECK(prefabs.refresh() == 1);
    sync_prefabs(w, prefabs, &errors);
    REQUIRE(errors.empty());
    CHECK_FALSE(w.alive(bulb));
    bulb = part(w, lamp, "bulb");
    CHECK(w.get<Light>(bulb)->intensity == 99.0f); // the override survived
    CHECK(w.get<Light>(bulb)->color.y == 1.0f);     // the prefab's new value
    CHECK(w.get<Transform>(bulb)->position.y == 3.0f);
    CHECK(w.get<Transform>(lamp)->position.z == 9.0f);
    CHECK(w.get<Target>(part(w, lamp, "shade"))->entity == target); // outside references survive too
    CHECK(w.query<const PrefabPart>().count() == 2);

    // Removing PrefabInstance unspawns; the root keeps its components.
    w.remove<PrefabInstance>(lamp);
    sync_prefabs(w, prefabs);
    CHECK(w.query<const PrefabPart>().count() == 0);
    CHECK(w.has<Name>(lamp));
}
