// Feature packages (M11.6): manifests, install order, boundaries.
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/app/feature_check.hpp>
#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <fstream>

using namespace astral;

namespace {

struct Score {
    std::int32_t points = 0;
};
struct Scored {
    std::int32_t by = 0;
};

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

FeatureManifest manifest(std::string name, std::vector<std::string> deps = {}) {
    FeatureManifest m;
    m.name = std::move(name);
    m.depends_on = std::move(deps);
    return m;
}

bool contains(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

} // namespace
ASTRAL_REFLECT(Score, points)
ASTRAL_REFLECT(Scored, by)

TEST_CASE("a feature.json is parsed and bad ones are explained") {
    std::string error;
    const auto doc = Json::parse(R"({"format": "astral.feature", "version": 1, "name": "combat", "description": "hits",
        "depends_on": ["health"], "components": ["game::Weapon"], "events": ["game::Hit"],
        "exports": {"headers": ["events.hpp"]}, "scripts": ["features/combat/combat.luau"], "settings": {"damage": 12}})");
    const auto m = parse_feature_manifest(*doc, &error);
    REQUIRE_MESSAGE(m, error);
    CHECK(m->name == "combat");
    CHECK(m->depends_on == std::vector<std::string>{"health"});
    CHECK(m->headers == std::vector<std::string>{"events.hpp"});
    CHECK(m->settings["damage"].as_number() == 12);
    CHECK_FALSE(parse_feature_manifest(Json::parse(R"({"name": "x"})").value(), &error));
    CHECK(contains(error, "astral.feature"));
    CHECK_FALSE(parse_feature_manifest(Json::parse(R"({"format": "astral.feature", "name": "x", "depends_on": "health"})").value(), &error));
    CHECK(contains(error, "depends_on"));
}

TEST_CASE("install order puts dependencies first and keeps the added order otherwise") {
    std::string error;
    const std::vector<FeatureManifest> list{manifest("ui", {"combat"}), manifest("audio"), manifest("combat", {"health"}), manifest("health")};
    const auto order = FeatureSet::order(list, &error);
    REQUIRE_MESSAGE(order, error);
    std::vector<std::string> names;
    for (std::size_t i : *order) names.push_back(list[i].name);
    CHECK(names == std::vector<std::string>{"health", "combat", "ui", "audio"});
}

TEST_CASE("cycles, missing dependencies and duplicates are errors that name the features") {
    std::string error;
    CHECK_FALSE(FeatureSet::order({manifest("a", {"b"}), manifest("b", {"c"}), manifest("c", {"a"})}, &error));
    CHECK(contains(error, "cycle"));
    CHECK(contains(error, "a -> b -> c -> a"));
    CHECK_FALSE(FeatureSet::order({manifest("a", {"ghost"})}, &error));
    CHECK(contains(error, "\"ghost\""));
    CHECK_FALSE(FeatureSet::order({manifest("a"), manifest("a")}, &error));
    CHECK(contains(error, "twice"));
}

TEST_CASE("install registers components, events and scripts; the manifest must list what install registers") {
    const auto dir = test::temp_dir("app_features");
    write_text(dir / "features/scoring/feature.json", R"({"format": "astral.feature", "name": "scoring",
        "components": ["Score"], "events": ["Scored"], "scripts": ["features/scoring/score.luau"], "settings": {"start": 5}})");
    write_text(dir / "features/scoring/score.luau",
               "astral.on('Scored', function(ctx, ev) local s = astral.find('board'):get('Score'); s.points = s.points + ev.by end)");
    write_text(dir / "features/liar/feature.json", R"({"format": "astral.feature", "name": "liar", "components": []})");

    struct G final : Game {
        bool ok = false;
        std::vector<std::string> order;
        double start = 0;
        int frame = 0;
        std::string api_folder;
        bool api_written = false;
        void on_update(Context& ctx) override {
            if (++frame == 2) ctx.world.send(Scored{3});
        }
        void on_start(Context& ctx) override {
            FeatureSet set;
            set.add("scoring", [&](FeatureContext& f) {
                f.component<Score>();
                f.event<Scored>();
                start = f.number("start", 0);
                order.push_back("scoring");
            });
            ok = set.install(ctx);
            api_written = ctx.write_script_api(api_folder);
            ctx.world.create(Name{"board"}, Score{});
        }
    } game;
    game.api_folder = (dir / "api_out").string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 6);
    CHECK(game.ok);
    CHECK(game.api_written);
    std::ifstream api_md(dir / "api_out" / "API.md");
    const std::string api_text((std::istreambuf_iterator<char>(api_md)), std::istreambuf_iterator<char>());
    CHECK(contains(api_text, "### `Score` (feature `scoring`)"));
    CHECK(std::filesystem::exists(dir / "api_out" / "astral.d.luau"));
    CHECK(game.start == 5);
    CHECK(app.context().types.find("Score"));
    CHECK(app.context().events.find("Scored"));
    CHECK(app.world().get<Score>(find_by_name(app.world(), "board"))->points == 3); // the feature's script ran

    struct Liar final : Game {
        bool ok = true;
        void on_start(Context& ctx) override {
            FeatureSet set;
            set.add("liar", [](FeatureContext& f) { f.component<Score>(); }); // not in feature.json
            ok = set.install(ctx);
        }
    } liar;
    App app2({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app2.run_frames(liar, 1);
    CHECK_FALSE(liar.ok);
}

TEST_CASE("the boundary check finds cross-feature includes, internal headers and global state") {
    const auto root = test::temp_dir("app_feature_check");
    const auto src = root / "src", man = root / "assets";
    write_text(man / "combat/feature.json", R"({"format": "astral.feature", "name": "combat", "depends_on": ["health"]})");
    write_text(man / "health/feature.json", R"({"format": "astral.feature", "name": "health", "exports": {"headers": ["events.hpp"]}})");
    write_text(man / "audio/feature.json", R"({"format": "astral.feature", "name": "audio"})");

    SUBCASE("a clean game passes") {
        write_text(src / "combat/combat.cpp", "#include \"features/health/events.hpp\"\n#include <astral/astral.hpp>\nstatic constexpr int kMax = 3;\nstatic int helper(int x) { return x; }\n");
        write_text(src / "health/events.hpp", "struct Damaged {};\n");
        write_text(src / "audio/audio.cpp", "// nothing\n");
        CHECK(check_features(src, man).empty());
    }
    SUBCASE("including a feature you do not depend on") {
        write_text(src / "combat/combat.cpp", "#include \"features/audio/sounds.hpp\"\n");
        write_text(src / "health/events.hpp", "");
        write_text(src / "audio/sounds.hpp", "");
        const auto problems = check_features(src, man);
        REQUIRE(problems.size() == 1);
        CHECK(contains(problems[0], "without depending"));
        CHECK(contains(problems[0], "combat.cpp:1"));
    }
    SUBCASE("including a header the other feature does not export") {
        write_text(src / "combat/combat.cpp", "#include \"features/health/internal.hpp\"\n");
        write_text(src / "health/events.hpp", "");
        write_text(src / "audio/a.cpp", "");
        const auto problems = check_features(src, man);
        REQUIRE(problems.size() == 1);
        CHECK(contains(problems[0], "not exported"));
    }
    SUBCASE("internal engine headers and mutable globals") {
        write_text(src / "combat/combat.cpp", "#include <astral/world/detail/archetype.hpp>\nstatic int counter = 0;\nstatic std::vector<int> cache;\nstatic int allowed = 1; // astral-allow-global: test\n");
        write_text(src / "health/events.hpp", "");
        write_text(src / "audio/a.cpp", "");
        const auto problems = check_features(src, man);
        REQUIRE(problems.size() == 3);
        CHECK(contains(problems[0], "internal engine header"));
        CHECK(contains(problems[1], "mutable global state"));
    }
    SUBCASE("a folder without a manifest, and a dependency cycle") {
        write_text(src / "ghost/a.cpp", "");
        write_text(man / "health/feature.json", R"({"format": "astral.feature", "name": "health", "depends_on": ["combat"]})");
        const auto problems = check_features(src, man);
        bool no_manifest = false, cycle = false;
        for (const auto& p : problems) {
            no_manifest = no_manifest || contains(p, "no feature.json");
            cycle = cycle || contains(p, "cycle");
        }
        CHECK(no_manifest);
        CHECK(cycle);
    }
}
