#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <vector>

using namespace astral;

namespace {
AppDesc headless() { return {.headless = true, .fixed_timestep = 1.0f / 60.0f}; }

struct Health {
    int value = 100;
};
struct Damage {
    Entity target;
    int amount = 0;
};
} // namespace

ASTRAL_REFLECT(Health, value)

namespace {

/// Counts Health changes it has not seen yet.
class Watcher final : public System {
public:
    std::vector<std::size_t> seen;
    void update(Context& ctx) override { seen.push_back(ctx.world.query<const Health>().changed<Health>().count()); }
};

/// Applies Damage events (mutably touching Health).
class DamageSystem final : public System {
public:
    EventReader<Damage> reader;
    void update(Context& ctx) override {
        reader.read(ctx.world, [&](const Damage& d) {
            if (Health* h = ctx.world.get<Health>(d.target)) h->value -= d.amount;
        });
    }
};

} // namespace

TEST_CASE("systems see component changes made since they last ran, and events flow between frames") {
    App app(headless());
    struct G final : Game {
        Entity a{}, b{};
        Watcher* watcher = nullptr;
        int frame = 0;
        void on_start(Context& ctx) override {
            a = ctx.world.create(Health{});
            b = ctx.world.create(Health{});
            ctx.add_system<DamageSystem>(Phase::PreUpdate);
            watcher = &ctx.add_system<Watcher>(Phase::Update);
        }
        void on_update(Context& ctx) override {
            if (frame == 2) ctx.world.send(Damage{b, 30}); // applied next frame by DamageSystem
            ++frame;
        }
    } game;
    app.run_frames(game, 5);
    REQUIRE(game.watcher->seen.size() == 5);
    CHECK(game.watcher->seen[0] == 2); // creation counts as a change the first time
    CHECK(game.watcher->seen[1] == 0);
    CHECK(game.watcher->seen[2] == 0);
    CHECK(game.watcher->seen[3] == 1); // the damage event changed b's Health
    CHECK(game.watcher->seen[4] == 0);
}

TEST_CASE("engine and game components save and load through ctx.types") {
    App app(headless());
    struct G final : Game {
        Json saved;
        LoadResult loaded;
        void on_start(Context& ctx) override {
            ctx.types.add<Health>();
            ctx.world.create(Transform{.position = {1, 2, 3}}, Health{42},
                             PointLight{.color = {1, 0.5f, 0.2f}, .intensity = 20, .radius = 8});
            ctx.world.create(Transform{}, Camera{.fov_y = 1.0f},
                             RectLight{.right = {2, 0, 0}, .up = {0, 0, 1}, .intensity = 15, .two_sided = true});
            ctx.world.create(Sdf{.shape = {.primitive = SdfPrimitive::Torus, .size = {1, 0.2f, 0, 0}}, .order = 3});
            saved = save_world(ctx.world, ctx.types);
            World copy;
            loaded = load_world(copy, ctx.types, saved);
            CHECK(save_world(copy, ctx.types) == saved);
        }
    } game;
    app.run_frames(game, 1);
    CHECK(game.loaded.errors.empty());
    CHECK(game.saved["entities"].size() == 3);
    const Json& first = game.saved["entities"][0]["components"];
    CHECK(first["Health"]["value"].as_number() == 42.0);
    CHECK(first["astral::PointLight"]["radius"].as_number() == 8.0);
    CHECK(game.saved["entities"][2]["components"]["astral::Sdf"]["shape"]["primitive"].as_number() ==
          static_cast<double>(SdfPrimitive::Torus));
}
