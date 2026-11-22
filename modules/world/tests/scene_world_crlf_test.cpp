#include <astral/world/map.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <string_view>

using namespace astral;

namespace {

std::string to_crlf(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 64);
    for (char c : text) {
        if (c == '\n') out += "\r\n";
        else if (c != '\r') out += c;
    }
    return out;
}

Json must_parse(std::string_view text) {
    std::string error;
    auto doc = Json::parse(text, &error);
    REQUIRE_MESSAGE(doc.has_value(), error);
    return *doc;
}

} // namespace

TEST_CASE("scene and prefab files with CRLF line endings parse identically to LF") {
    const std::string prefab_lf =
        "{\n"
        "  \"format\": \"astral.scene\",\n"
        "  \"version\": 1,\n"
        "  \"entities\": [\n"
        "    {\"id\": 1, \"components\": {\"astral::Name\": {\"value\": \"lamp\"}, \"astral::Transform\": {}}},\n"
        "    {\"id\": 2, \"components\": {\"astral::Name\": {\"value\": \"bulb\"}, \"astral::Parent\": {\"entity\": 1},\n"
        "                               \"astral::Transform\": {\"position\": [0, 2, 0]}}}\n"
        "  ]\n"
        "}\n";
    const std::string scene_lf =
        "{\n"
        "  \"format\": \"astral.scene\",\n"
        "  \"version\": 1,\n"
        "  \"entities\": [\n"
        "    {\"id\": 1, \"prefab\": \"lamp.prefab\",\n"
        "     \"components\": {\"astral::Transform\": {\"position\": [5, 0, -2]}}},\n"
        "    {\"id\": 2, \"components\": {\"astral::Name\": {\"value\": \"floor\"}, \"astral::Transform\": {}}}\n"
        "  ]\n"
        "}\n";

    auto load_and_save = [](std::string_view prefab_text, std::string_view scene_text) {
        TypeRegistry types;
        register_world_types(types);
        std::map<std::string, Json> docs;
        docs.emplace("lamp.prefab", must_parse(prefab_text));
        DocumentSource src = [&docs](std::string_view path) {
            auto it = docs.find(std::string(path));
            if (it == docs.end()) return SourceDocument{.error = "no such file"};
            return SourceDocument{.json = &it->second, .version = 1, .error = {}};
        };
        PrefabLibrary prefabs(types, src);
        World w;
        const LoadResult r = load_scene(w, prefabs, must_parse(scene_text));
        REQUIRE_MESSAGE(r.errors.empty(), (r.errors.empty() ? "" : r.errors[0]));
        return save_scene(w, prefabs);
    };

    const Json saved_lf = load_and_save(prefab_lf, scene_lf);
    const Json saved_crlf = load_and_save(to_crlf(prefab_lf), to_crlf(scene_lf));
    CHECK(saved_crlf == saved_lf);
}

TEST_CASE("world (.world and .ent) files with CRLF line endings parse identically to LF") {
    const std::string world_lf =
        "{\n"
        "  \"format\": \"astral.map\",\n"
        "  \"version\": 1,\n"
        "  \"settings\": {\"gravity\": -9.81},\n"
        "  \"grids\": [\n"
        "    {\"name\": \"main\", \"cell_size\": 100, \"load_range\": 300}\n"
        "  ],\n"
        "  \"layers\": [\n"
        "    {\"name\": \"night\", \"active\": false}\n"
        "  ]\n"
        "}\n";
    const std::string ent_a_lf =
        "{\n"
        "  \"format\": \"astral.entity\",\n"
        "  \"version\": 1,\n"
        "  \"entity\": {\n"
        "    \"id\": 1,\n"
        "    \"components\": {\n"
        "      \"astral::Name\": {\"value\": \"pillar\"},\n"
        "      \"astral::Transform\": {\"position\": [25.5, 0, -10]}\n"
        "    }\n"
        "  },\n"
        "  \"layers\": [\"night\"]\n"
        "}\n";
    const std::string ent_b_lf =
        "{\n"
        "  \"format\": \"astral.entity\",\n"
        "  \"version\": 1,\n"
        "  \"entity\": {\n"
        "    \"id\": 2,\n"
        "    \"components\": {\"astral::Name\": {\"value\": \"rules\"}}\n"
        "  },\n"
        "  \"always_loaded\": true\n"
        "}\n";

    auto cook = [](std::string_view map_text, std::string_view a_text, std::string_view b_text) {
        std::string error;
        auto map = parse_map(must_parse(map_text), &error);
        REQUIRE_MESSAGE(map.has_value(), error);
        std::vector<EntityFile> files;
        files.push_back({"1.ent", must_parse(a_text)});
        files.push_back({"2.ent", must_parse(b_text)});
        const MapSource src = read_map_source(*map, files);
        REQUIRE(src.errors.empty());
        return std::pair{write_map(*map), cook_map(src)};
    };

    const auto [map_lf, cooked_lf] = cook(world_lf, ent_a_lf, ent_b_lf);
    const auto [map_crlf, cooked_crlf] = cook(to_crlf(world_lf), to_crlf(ent_a_lf), to_crlf(ent_b_lf));

    CHECK(map_crlf == map_lf);
    CHECK(cooked_crlf.index == cooked_lf.index);
    REQUIRE(cooked_crlf.cells.size() == cooked_lf.cells.size());
    for (std::size_t i = 0; i < cooked_lf.cells.size(); ++i) {
        CHECK(cooked_crlf.cells[i].name == cooked_lf.cells[i].name);
        CHECK(cooked_crlf.cells[i].document == cooked_lf.cells[i].document);
    }
}
