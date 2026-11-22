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
    CHECK(game.saved["entities"][2]["components"]["astral::Sdf"]["shape"]["primitive"].as_string() == "Torus"); // enums by name
}

namespace {
struct PosX {
    float x = 0.0f;
};
struct PosY {
    float y = 0.0f;
};
struct VelX {
    float dx = 1.0f;
};
struct VelY {
    float dy = 2.0f;
};
struct Combined {
    float sum = 0.0f;
};

class StepXSystem final : public System {
public:
    void configure(SystemAccess& access) override { access.query<PosX, const VelX>(); }
    void update(Context& ctx) override {
        ctx.world.each<PosX, const VelX>([&](PosX& p, const VelX& v) { p.x += v.dx; });
    }
};

class StepYSystem final : public System {
public:
    void configure(SystemAccess& access) override { access.query<PosY, const VelY>(); }
    void update(Context& ctx) override {
        ctx.world.each<PosY, const VelY>([&](PosY& p, const VelY& v) { p.y += v.dy; });
    }
};

class CombineXYSystem final : public System {
public:
    std::size_t changed_x_total = 0;
    std::size_t changed_y_total = 0;
    void configure(SystemAccess& access) override { access.query<const PosX, const PosY, Combined>(); }
    void update(Context& ctx) override {
        changed_x_total += ctx.world.query<const PosX>().changed<PosX>().count();
        changed_y_total += ctx.world.query<const PosY>().changed<PosY>().count();
        ctx.world.each<const PosX, const PosY, Combined>(
            [&](const PosX& x, const PosY& y, Combined& c) { c.sum += x.x + y.y; });
    }
};
} // namespace

TEST_CASE("SystemAccess conflict rules distinguish shared reads from read-write and exclusive systems") {
    SystemAccess read_xy;
    read_xy.reads<PosX, PosY>();

    SystemAccess read_x_only;
    read_x_only.reads<PosX>();
    CHECK_FALSE(read_xy.conflicts_with(read_x_only));

    SystemAccess write_x;
    write_x.query<PosX, const VelX>();
    CHECK(read_xy.conflicts_with(write_x));
    CHECK(write_x.conflicts_with(read_x_only));

    SystemAccess write_y;
    write_y.query<PosY, const VelY>();
    CHECK_FALSE(write_x.conflicts_with(write_y));

    SystemAccess undeclared;
    CHECK(undeclared.exclusive());
    CHECK(undeclared.conflicts_with(write_x));
}

TEST_CASE("parallel wave scheduler matches serial execution and change detection over 200 frames") {
    auto run_simulation = [](bool parallel) {
        AppDesc desc = headless();
        desc.parallel_systems = parallel;
        App app(desc);
        struct SimGame final : Game {
            CombineXYSystem* combiner = nullptr;
            double total_sum = 0.0;
            void on_start(Context& ctx) override {
                for (int i = 0; i < 256; ++i) {
                    const float fi = static_cast<float>(i + 1);
                    ctx.world.create(PosX{fi}, PosY{fi * 0.5f}, VelX{0.25f}, VelY{0.75f}, Combined{0.0f});
                }
                ctx.add_system<StepXSystem>(Phase::FixedUpdate);
                ctx.add_system<StepYSystem>(Phase::FixedUpdate);
                combiner = &ctx.add_system<CombineXYSystem>(Phase::FixedUpdate);
            }
            void on_shutdown(Context& ctx) override {
                ctx.world.each<const Combined>([&](const Combined& c) { total_sum += c.sum; });
            }
        } game;
        app.run_frames(game, 200);
        return std::tuple{game.total_sum, game.combiner->changed_x_total, game.combiner->changed_y_total};
    };

    const auto [serial_sum, serial_cx, serial_cy] = run_simulation(false);
    const auto [parallel_sum, parallel_cx, parallel_cy] = run_simulation(true);
    CHECK(parallel_sum == doctest::Approx(serial_sum));
    CHECK(parallel_cx == serial_cx);
    CHECK(parallel_cy == serial_cy);
    CHECK(parallel_cx == 256u * 200u);
    CHECK(parallel_cy == 256u * 200u);
}
