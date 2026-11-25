#include "gltf_writer.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <astral/assets/terrain.hpp>
#include <doctest/doctest.h>

#include <fstream>

using namespace astral;

namespace {
const std::vector<std::uint8_t> ramp_png{
    137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72,
    68, 82, 0, 0, 0, 5, 0, 0, 0, 5, 16, 0, 0, 0,
    0, 248, 148, 165, 122, 0, 0, 0, 47, 73, 68, 65, 84, 120,
    156, 5, 193, 11, 18, 64, 32, 20, 0, 192, 109, 140, 8, 249,
    22, 247, 63, 233, 179, 11, 73, 36, 67, 48, 202, 38, 179, 194,
    98, 181, 169, 118, 14, 167, 203, 237, 161, 233, 94, 95, 196, 15,
    116, 87, 5, 12, 25, 223, 205, 18, 0, 0, 0, 0, 73, 69,
    78, 68, 174, 66, 96, 130,
};
} // namespace

TEST_CASE("terrain import preserves 16-bit samples and row direction") {
    const auto tile = read_terrain_tile(ramp_png, 5);
    REQUIRE(tile);
    REQUIRE(tile->size() == 25);
    CHECK((*tile)[0] == 0);
    CHECK((*tile)[1] == 1);
    CHECK((*tile)[2] == 255);
    CHECK((*tile)[3] == 256);
    CHECK((*tile)[5] == 5);
    CHECK(tile->back() == 65535);
    TerrainManifest range{.min_height_m = -10, .max_height_m = 30, .tiles = {}};
    CHECK(range.height_m(tile->front()) == -10);
    CHECK(range.height_m(tile->back()) == 30);
    CHECK_FALSE(read_terrain_tile(ramp_png, 9));
    auto wrong_depth = ramp_png;
    wrong_depth[24] = 8;
    CHECK_FALSE(read_terrain_tile(wrong_depth, 5));
    auto truncated = write_terrain_tile(*tile);
    truncated.pop_back();
    CHECK_FALSE(read_terrain_tile(truncated, 5));
}

TEST_CASE("terrain cook preserves tiles in packs and checks shared borders on recook") {
    const auto dir = test::temp_dir("assets_terrain");
    const auto source = dir / "source";
    const auto output = dir / "cooked";
    std::filesystem::create_directories(source);
    std::ofstream(source / "land.aterrain") << R"({
        "format":"astral.terrain","version":1,"samples":5,"spacing_m":2,
        "min_height_m":-10,"max_height_m":30,
        "tiles":[{"x":-1,"z":0,"file":"west.png"},{"x":0,"z":0,"file":"east.aheight"}]
    })";
    test::write_bytes(source / "west.png", ramp_png);
    const auto west = read_terrain_tile(ramp_png, 5);
    REQUIRE(west);
    auto east = *west;
    for (std::size_t row = 0; row < 5; ++row) east[row * 5] = (*west)[row * 5 + 4];
    test::write_bytes(source / "east.aheight", write_terrain_tile(east));

    REQUIRE(cook_directory({.source = source, .output = output}).ok());
    CHECK(cook_directory({.source = source, .output = output}).up_to_date == 3);
    REQUIRE(write_pack(output, dir / "terrain.apak"));
    AssetManager packed({.root = dir / "absent", .hot_reload = false, .pack = dir / "terrain.apak"});
    const auto manifest_bytes = packed.read("land.aterrain");
    REQUIRE(manifest_bytes);
    const auto manifest = read_terrain_manifest(*manifest_bytes);
    REQUIRE(manifest);
    REQUIRE(manifest->tiles.size() == 2);
    CHECK(manifest->tiles.front().x == -1);
    CHECK(manifest->spacing_m == 2);
    const auto tile_bytes = packed.read(manifest->tiles.front().file);
    REQUIRE(tile_bytes);
    CHECK(read_terrain_tile(*tile_bytes, manifest->samples) == west);

    std::filesystem::remove(output / manifest->tiles.front().file);
    CHECK(cook_directory({.source = source, .output = output}).cooked == 1);
    east[0] ^= 1;
    test::write_bytes(source / "east.aheight", write_terrain_tile(east));
    const auto invalid = cook_directory({.source = source, .output = output});
    REQUIRE_FALSE(invalid.ok());
    CHECK(invalid.errors.front().find("shared boundary") != std::string::npos);
    // A failed cook keeps the last good output (MSR AS09); the cook still reports the error.
    CHECK(std::filesystem::exists(output / "land.aterrain.tiles"));
}

TEST_CASE("terrain manifests reject ambiguous tile addresses and paths") {
    const auto read = [](std::string_view tiles) {
        const std::string manifest =
            R"({"format":"astral.terrain","version":1,"samples":65,"spacing_m":1,"min_height_m":0,"max_height_m":50,"tiles":)"
            + std::string(tiles) + "}";
        return read_terrain_manifest(std::span(reinterpret_cast<const std::uint8_t*>(manifest.data()), manifest.size()));
    };
    CHECK(read(R"([{"x":-3,"z":2,"file":"tiles/rock.png"}])").has_value());
    CHECK_FALSE(read(R"([{"x":-3,"z":2,"file":"tiles/rock.png"},{"x":-3,"z":2,"file":"sand.png"}])"));
    CHECK_FALSE(read(R"([{"x":0.5,"z":0,"file":"rock.png"}])"));
    CHECK_FALSE(read(R"([{"x":0,"z":0,"file":"../rock.png"}])"));
}
