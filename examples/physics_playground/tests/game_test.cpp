// physics_playground, headless: things float and sink, the bridge holds, the pad throws, the wind turns.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/playground/playground.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the lake floats the crates and sinks the stones; the bridge holds; the wind turns; no script errors") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    physics_playground::PlaygroundGame game;
    app.run_frames(game, 60 * 8);
    World& w = app.world();
    CHECK(app.context().scripts->take_errors().empty());

    float crate_lowest = 100.0f, crate_highest = -100.0f, stone_highest = -100.0f;
    std::size_t crates = 0, stones = 0;
    w.each<const RigidBody, const Transform, const Buoyant>([&](const RigidBody& rb, const Transform& t, const Buoyant& b) {
        if (rb.desc.shape.type != ShapeType::Box || b.volume < 0.2f) return;
        if (rb.desc.mass == 90.0f && t.position.z < 0) { // the six crates on the lake
            ++crates;
            crate_lowest = std::min(crate_lowest, t.position.y);
            crate_highest = std::max(crate_highest, t.position.y);
        } else if (rb.desc.mass == 420.0f) {
            ++stones;
            stone_highest = std::max(stone_highest, t.position.y);
        }
    });
    CHECK(crates == 6);
    CHECK(crate_lowest > -0.6f);  // floating at the surface (y = 0), not on the bottom
    CHECK(crate_highest < 0.6f);
    CHECK(stones == 3);
    CHECK(stone_highest < -1.0f); // sunk

    // The plank bridge sags under its own weight but holds: every plank is above the lake bottom and near the bridge height.
    float lowest_plank = 100.0f;
    std::size_t planks = 0;
    w.each<const RigidBody, const Transform>([&](const RigidBody& rb, const Transform& t) {
        if (rb.desc.shape.type == ShapeType::Box && rb.desc.mass == 20.0f) {
            ++planks;
            lowest_plank = std::min(lowest_plank, t.position.y);
        }
    });
    CHECK(planks == 14);
    CHECK(lowest_plank > physics_playground::terrain_height(-14.0f, 17.0f) - 1.5f);

    CHECK(w.count<Joint>() == 15);
    CHECK(app.context().physics->body_count() > 25);
    const Vec3 wind = app.context().fields.wind_at({0, 5, 0}, app.time().elapsed);
    CHECK(length(wind) > 1.0f);

    // F1: Luau Contact & TriggerEntered listeners recorded real physics events in reflected state.
    const Entity stats_entity = find_by_name(w, "playground_stats");
    REQUIRE(stats_entity.valid());
    const auto* stats = w.get<const playground::ContactStats>(stats_entity);
    REQUIRE(stats != nullptr);
    CHECK(stats->begin_count > 0);
    CHECK(stats->pad_launches > 0);
}

TEST_CASE("physics_playground supports Luau queued forces and contact listening on dynamic bodies") {
    struct ForceGame final : Game {
        physics_playground::PlaygroundGame base;
        Entity probe{};
        void on_start(Context& ctx) override {
            base.on_start(ctx);
            probe = ctx.world.create(Name{"luau_force_probe"}, Transform{.position = {0, 20.0f, 0}},
                                     RigidBody{.desc = {.shape = ShapeDesc::sphere(0.4f), .mass = 2.0f}});
            REQUIRE(ctx.scripts->load("f1_force_rule.luau", R"(
                astral.system {
                    phase = "FixedUpdate",
                    run = function()
                        local e = astral.find("luau_force_probe")
                        if e then
                            astral.physics.force(e, vector.create(40, 0, 0))
                        end
                    end
                }
            )").empty());
        }
        void on_update(Context& ctx) override { base.on_update(ctx); }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    app.run_frames(game, 30);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().get<const Transform>(game.probe)->position.x > 0.5f);
    CHECK(app.context().physics_service().linear_velocity(game.probe).x > 2.0f);
}
