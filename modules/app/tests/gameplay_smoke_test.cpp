#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace astral;

TEST_CASE("gameplay smoke integrates the actual length inside occupied cells") {
    const GameplaySmoke smoke({.radius_cells = 2, .cell_size_m = 0.5f, .extinction_per_m = 4});
    const Vec3 centre{-100, 20, 30};
    const Vec3 left = centre + Vec3{-2, 0, 0};
    const Vec3 right = centre + Vec3{2, 0, 0};
    CHECK(smoke.optical_depth(centre, left, right) == doctest::Approx(2));
    CHECK(smoke.optical_depth(centre, right, left) == doctest::Approx(2));
    CHECK(smoke.optical_depth(centre, centre, right) == doctest::Approx(1));
    CHECK(smoke.optical_depth(centre, centre, centre) == 0);
    CHECK(smoke.optical_depth(centre, left + Vec3{0, 2, 0}, right + Vec3{0, 2, 0}) == 0);
    CHECK(smoke.transmittance(centre, left, right) == doctest::Approx(std::exp(-2.0f)));
    const Vec3 diagonal{2, 2, 2};
    CHECK(smoke.optical_depth(centre, centre - diagonal, centre + diagonal) == doctest::Approx(2 * std::sqrt(3.0f)));
}

namespace {
struct CorridorSmoke final : Game {
    bool full_partition;
    Entity door{}, smoke{};
    unsigned frame = 0;
    std::vector<float> samples;
    std::vector<std::uint8_t> final_occupancy;

    explicit CorridorSmoke(bool full) : full_partition(full) {}

    void on_start(Context& ctx) override {
        const Vec3 half = full_partition ? Vec3{0.08f, 2, 2} : Vec3{0.08f, 2, 0.55f};
        door = ctx.world.create(Transform{.position = {0.25f, 0, 0}},
            RigidBody{.desc = {.shape = ShapeDesc::box(half), .motion = MotionType::Static}});
        smoke = ctx.world.create(Transform{}, GameplaySmoke({.radius_cells = 3, .cell_size_m = 0.5f,
            .extinction_per_m = 2, .spread_interval_steps = 1, .spread_steps = 32, .fade_steps = 6}));
    }

    void on_update(Context& ctx) override {
        ++frame;
        const auto& grid = *ctx.world.get<const GameplaySmoke>(smoke);
        const Vec3 start{0.51f, 0, 0};
        const Vec3 end{2, 0, 0};
        samples.push_back(grid.transmittance({}, start, end));
        if (frame == 13 && full_partition) ctx.physics_service().set_body_transform(door, {0.25f, 0, 8}, {});
        if (frame == 26) final_occupancy.assign(grid.occupancy().begin(), grid.occupancy().end());
    }
};
} // namespace

TEST_CASE("gameplay smoke follows completed fixed-step door motion and fades deterministically") {
    const auto run = [] {
        CorridorSmoke game(true);
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .worker_threads = 2, .hot_reload = false});
        app.run_frames(game, 41);
        return game;
    };
    const auto a = run();
    const auto b = run();
    REQUIRE(a.samples.size() == 41);
    CHECK(a.samples[12] == 1.0f);
    CHECK(a.samples[13] == 1.0f); // the command does not retroactively change the previous flood fill
    CHECK(a.samples[25] < 0.2f);
    CHECK(a.samples[35] > a.samples[31]);
    CHECK(a.samples.back() == 1.0f);
    CHECK(a.samples == b.samples);
    CHECK(a.final_occupancy == b.final_occupancy);
}

TEST_CASE("gameplay smoke reaches the other side around the end of a partition") {
    CorridorSmoke game(false);
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .worker_threads = 2, .hot_reload = false});
    app.run_frames(game, 27);
    REQUIRE(game.samples.size() == 27);
    CHECK(game.samples[2] == 1.0f);
    CHECK(game.samples[25] < 0.2f);
}
