#include "../../assets/tests/gltf_writer.hpp"

#include <astral/assets/cook.hpp>
#include <astral/astral.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

void write_entity(const std::filesystem::path& dir, int id, const char* name, double x, double z, const char* extra = "") {
    write_text(dir / (std::to_string(id) + ".ent"),
               std::string(R"({"format":"astral.entity","version":1,"entity":{"id":)") + std::to_string(id) +
                   R"(,"components":{"astral::Name":{"value":")" + name + R"("},"astral::Transform":{"position":[)" +
                   std::to_string(x) + ",0," + std::to_string(z) + "]}}}" + extra + "}");
}

/// A 3-cell strip map: "near" at the start, "far" 1 km away, a "lamp" on the night layer, "rules" always.
std::filesystem::path make_map(const char* name) {
    const auto dir = test::temp_dir(name);
    write_text(dir / "maps" / "strip.world", R"({"format":"astral.map","version":1,
        "grids":[{"name":"main","cell_size":100,"load_range":150}],
        "layers":[{"name":"night","active":false}]})");
    const auto ents = dir / "maps" / "strip.entities";
    write_entity(ents, 1, "near", 50, 50);
    write_entity(ents, 2, "far", 1050, 50);
    write_entity(ents, 3, "lamp", 60, 40, R"(,"layers":["night"])");
    write_entity(ents, 4, "rules", 0, 0, R"(,"always_loaded":true)");
    return dir;
}

struct Explorer final : Game {
    bool opened = false;
    Entity source{};
    bool near_at_start = false, far_at_start = true, rules = false, lamp_off = true, lamp_on = false;
    bool far_later = false, near_later = true;
    double origin_x = 0;
    float source_local_x = 0;
    StreamingStats stats{};
    int shifted_events = 0;
    EventReader<OriginShifted> shifts;

    void on_start(Context& ctx) override {
        opened = ctx.open_world("maps/strip.world");
        source = ctx.world.create(Transform{}, StreamingSource{});
        ctx.finish_world_loading();
        near_at_start = find_by_name(ctx.world, "near").valid();
        far_at_start = find_by_name(ctx.world, "far").valid();
        rules = find_by_name(ctx.world, "rules").valid();
        lamp_off = !find_by_name(ctx.world, "lamp").valid();
        REQUIRE(ctx.world_layers() != nullptr);
        ctx.world_layers()->set("night", true);
    }
    void on_update(Context& ctx) override {
        const std::uint64_t frame = ctx.time.frame;
        shifted_events += static_cast<int>(shifts.read(ctx.world, [](const OriginShifted&) {}));
        if (frame == 2) {
            lamp_on = find_by_name(ctx.world, "lamp").valid(); // layers refresh at the start of a frame
            ctx.world.get<Transform>(source)->position.x = 1050.0f;
        } else if (frame == 4) {
            ctx.finish_world_loading();
            far_later = find_by_name(ctx.world, "far").valid();
            near_later = find_by_name(ctx.world, "near").valid();
            ctx.world.get<Transform>(source)->position.x = 2550.0f; // past the 2 km origin threshold
        } else if (frame == 6) {
            origin_x = ctx.world_origin().x;
            source_local_x = ctx.world.get<Transform>(source)->position.x;
            stats = *ctx.world_streaming();
        }
    }
};

void check(const Explorer& g) {
    CHECK(g.opened);
    CHECK(g.near_at_start);
    CHECK_FALSE(g.far_at_start);
    CHECK(g.rules);
    CHECK(g.lamp_off);
    CHECK(g.lamp_on);
    CHECK(g.far_later);
    CHECK_FALSE(g.near_later);
    CHECK(g.origin_x == doctest::Approx(2600.0)); // snapped to 100 m
    CHECK(g.source_local_x == doctest::Approx(-50.0f));
    CHECK(g.shifted_events == 1);
    CHECK(g.stats.attached == 0); // nothing is placed near 2.5 km: the far cell was unloaded
}

} // namespace

TEST_CASE("open_world streams a source map around a StreamingSource, with layers and a floating origin") {
    const auto dir = make_map("app_world_source");
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string()});
    Explorer game;
    app.run_frames(game, 8);
    check(game);
}

TEST_CASE("open_world reads cooked cells the same way") {
    const auto dir = make_map("app_world_cooked_src");
    const auto out = test::temp_dir("app_world_cooked_out");
    REQUIRE(cook_directory({.source = dir, .output = out, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    REQUIRE(std::filesystem::exists(out / "maps" / "strip.cells" / "always.cell"));
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = out.string()});
    Explorer game;
    app.run_frames(game, 8);
    check(game);
}

TEST_CASE("open_world reports a missing or wrong map") {
    const auto dir = test::temp_dir("app_world_bad");
    write_text(dir / "maps" / "wrong.world", R"({"format":"astral.scene","version":1})");
    App app({.headless = true, .asset_root = dir.string()});
    struct G final : Game {
        bool missing = true, wrong = true;
        void on_start(Context& ctx) override {
            missing = ctx.open_world("maps/none.world");
            wrong = ctx.open_world("maps/wrong.world");
        }
    } game;
    app.run_frames(game, 1);
    CHECK_FALSE(game.missing);
    CHECK_FALSE(game.wrong);
}
