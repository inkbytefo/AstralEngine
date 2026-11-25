// G1.1 item 1: angular physics commands (torque, force at a point, angular impulse), angular velocity
// reads, and filtered queries (several ignored bodies, sensors) from C++ and Luau.
#include "../../assets/tests/gltf_writer.hpp"
#include "../src/services/physics_commands.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

RigidBody floating_ball(bool sensor = false) {
    return RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 1.0f, .gravity_factor = 0.0f, .sensor = sensor}};
}

struct Spin { int idx = 0; };

/// What the Luau test reports back.
struct Probe {
    float wa_y = 0, wb_x = 0, wc_z = 0, all = 0, skip = 0, sensor = 0;
};

} // namespace

ASTRAL_REFLECT(Probe, wa_y, wb_x, wc_z, all, skip, sensor)

namespace {

/// Queues all three angular commands from (possibly parallel) system jobs.
class SpinSystem final : public System {
public:
    void configure(SystemAccess& access) override { access.reads<Spin>(); }
    void update(Context& ctx) override {
        ctx.world.each<const Spin>([&](Entity e, const Spin& s) {
            const float k = static_cast<float>(s.idx + 1);
            ctx.physics_service().add_torque(e, {0.0f, k, 0.0f});
            ctx.physics_service().add_angular_impulse(e, {0.05f * k, 0.0f, 0.0f});
            const Vec3 at = ctx.world.get<const Transform>(e)->position + Vec3{0.0f, 0.5f, 0.0f};
            ctx.physics_service().add_force_at(e, {k, 0.0f, 0.0f}, at);
        });
    }
};

} // namespace

TEST_CASE("G1.1 PhysicsService torque, angular impulse and force at a point turn bodies") {
    struct G final : Game {
        Entity torque_ball{}, impulse_ball{}, lever_ball{};
        bool queued = false;
        void on_start(Context& ctx) override {
            torque_ball = ctx.world.create(Transform{.position = {0, 10, 0}}, floating_ball());
            impulse_ball = ctx.world.create(Transform{.position = {5, 10, 0}}, floating_ball());
            lever_ball = ctx.world.create(Transform{.position = {10, 10, 0}}, floating_ball());
        }
        void on_update(Context& ctx) override {
            if (queued) return;
            queued = true;
            PhysicsService& p = ctx.physics_service();
            p.add_torque(torque_ball, {0.0f, 2.0f, 0.0f});
            p.add_angular_impulse(impulse_ball, {0.0f, 0.0f, 0.3f});
            // +x force applied above the centre: r x F = (0,1,0) x (1,0,0) = (0,0,-1) -> spins about -z.
            p.add_force_at(lever_ball, {20.0f, 0.0f, 0.0f}, {10.0f, 11.0f, 0.0f});
        }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f});
    app.run_frames(game, 2);
    const PhysicsService& p = app.context().physics_service();
    CHECK(p.angular_velocity(game.torque_ball).y > 0.0f);
    CHECK(p.angular_velocity(game.impulse_ball).z > 0.0f);
    CHECK(p.angular_velocity(game.lever_ball).z < 0.0f);
    CHECK(p.linear_velocity(game.lever_ball).x > 0.0f);
    CHECK(p.angular_velocity(Entity{}) == Vec3{});
}

TEST_CASE("G1.1 angular commands from parallel and serial systems give identical simulation state") {
    auto run_sim = [](bool parallel) {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f, .parallel_systems = parallel});
        struct SimGame final : Game {
            std::vector<Entity> bodies;
            void on_start(Context& ctx) override {
                for (int i = 0; i < 12; ++i) {
                    bodies.push_back(ctx.world.create(Transform{.position = {static_cast<float>(i) * 3.0f, 4.0f, 0.0f}},
                                                      floating_ball(), Spin{i}));
                }
                ctx.add_system<SpinSystem>(Phase::FixedUpdate);
            }
        } game;
        app.run_frames(game, 5);
        std::vector<Vec3> out;
        for (Entity e : game.bodies) {
            out.push_back(app.context().physics_service().angular_velocity(e));
            out.push_back(app.world().get<const Transform>(e)->position);
        }
        return out;
    };
    const std::vector<Vec3> serial = run_sim(false);
    const std::vector<Vec3> parallel = run_sim(true);
    REQUIRE(serial.size() == parallel.size());
    for (std::size_t i = 0; i < serial.size(); ++i) CHECK(serial[i] == parallel[i]);
    CHECK(serial[0].y > 0.0f);
}

TEST_CASE("G1.1 queued force-at points move with the floating origin; force vectors do not") {
    PhysicsCommandQueue queue;
    queue.enqueue({.entity = Entity{1, 1}, .kind = PhysicsCommandKind::ForceAt, .value = {3, 0, 0}, .point = {2005, 1, -3000}});
    queue.shift_positions({2000, 0, -3000});
    std::vector<PhysicsCommand> out;
    queue.drain_sorted(out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].point.x == doctest::Approx(5.0f));
    CHECK(out[0].point.z == doctest::Approx(0.0f));
    CHECK(out[0].value.x == doctest::Approx(3.0f));
}

TEST_CASE("G1.1 queries skip every body in QueryFilter::ignore_list and see sensors only when asked") {
    struct G final : Game {
        Entity near_ball{}, mid_ball{}, far_ball{}, trigger{};
        void on_start(Context& ctx) override {
            trigger = ctx.world.create(Transform{.position = {0, 0, 2}}, floating_ball(true));
            near_ball = ctx.world.create(Transform{.position = {0, 0, 4}}, floating_ball());
            mid_ball = ctx.world.create(Transform{.position = {0, 0, 6}}, floating_ball());
            far_ball = ctx.world.create(Transform{.position = {0, 0, 8}}, floating_ball());
        }
    } game;
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f});
    app.run_frames(game, 2);
    PhysicsService& p = app.context().physics_service();
    const BodyHandle skip[] = {app.world().get<const RigidBody>(game.near_ball)->handle,
                               app.world().get<const RigidBody>(game.mid_ball)->handle};

    const auto plain = p.raycast({0, 0, 0}, {0, 0, 1}, 20.0f);
    REQUIRE(plain);
    CHECK(plain->distance == doctest::Approx(3.5f).epsilon(0.01));
    const auto skipped = p.raycast({0, 0, 0}, {0, 0, 1}, 20.0f, {.ignore_list = skip});
    REQUIRE(skipped);
    CHECK(skipped->distance == doctest::Approx(7.5f).epsilon(0.01));
    const auto sensed = p.raycast({0, 0, 0}, {0, 0, 1}, 20.0f, {.include_sensors = true});
    REQUIRE(sensed);
    CHECK(sensed->distance == doctest::Approx(1.5f).epsilon(0.01));
    const auto swept = p.spherecast({0, 0, 0}, 0.1f, {0, 0, 20}, {.ignore_list = skip});
    REQUIRE(swept);
    CHECK(swept->distance == doctest::Approx(7.4f).epsilon(0.02));
}

TEST_CASE("G1.1 Luau astral.physics angular commands, angular_velocity and filtered raycast") {
    const auto dir = test::temp_dir("g1_physics_angular_luau");
    write_text(dir / "spin.luau", R"(
        local frame = 0
        astral.system {
            phase = "FixedUpdate",
            run = function()
                frame += 1
                local a = astral.find("a")
                local b = astral.find("b")
                local c = astral.find("c")
                if frame == 1 then
                    astral.physics.torque(a, vector.create(0, 3, 0))
                    astral.physics.angular_impulse(b, vector.create(0.3, 0, 0))
                    astral.physics.force_at(c, vector.create(20, 0, 0), vector.create(10, 11, 0))
                elseif frame == 3 then
                    astral.physics.set_angular_velocity(a, vector.create(0, 0, 0))
                    local wa = astral.physics.angular_velocity(a)
                    local wb = astral.physics.angular_velocity(b)
                    local wc = astral.physics.angular_velocity(c)
                    local hit_all = astral.physics.raycast(vector.create(0, 10, -10), vector.create(0, 0, 1), 30)
                    local hit_skip = astral.physics.raycast(vector.create(0, 10, -10), vector.create(0, 0, 1), 30,
                        { ignore = { a } })
                    local hit_sensor = astral.physics.raycast(vector.create(0, 10, -10), vector.create(0, 0, 1), 30,
                        { ignore = a, sensors = true })
                    a:add("Probe", {
                        wa_y = wa.y, wb_x = wb.x, wc_z = wc.z,
                        all = hit_all and hit_all.distance or -1,
                        skip = hit_skip and hit_skip.distance or -1,
                        sensor = hit_sensor and hit_sensor.distance or -1,
                    })
                end
            end
        }
    )");
    write_text(dir / "bad_load.luau", R"(
        astral.physics.torque(astral.find("a"), vector.create(0, 1, 0))
    )");

    struct G final : Game {
        std::vector<std::string> load_errors;
        Entity a{};
        void on_start(Context& ctx) override {
            ctx.types.add<Probe>();
            a = ctx.world.create(Name{"a"}, Transform{.position = {0, 10, 0}}, floating_ball());
            ctx.world.create(Name{"b"}, Transform{.position = {0, 10, 5}}, floating_ball());
            ctx.world.create(Name{"c"}, Transform{.position = {10, 10, 0}}, floating_ball());
            ctx.world.create(Name{"trigger"}, Transform{.position = {0, 10, 2}}, floating_ball(true));
            load_errors = ctx.script_service().load_script("bad_load.luau");
            REQUIRE(ctx.script_service().load_script("spin.luau").empty());
        }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f, .asset_root = dir.string(),
             .hot_reload = false});
    app.run_frames(game, 5);
    CHECK_FALSE(game.load_errors.empty()); // commands are not allowed while a module loads (ADR-0044)
    CHECK(app.context().scripts->take_errors().empty());
    const Probe* probe = app.world().get<const Probe>(game.a);
    REQUIRE(probe);
    CHECK(probe->wa_y > 0.0f);
    CHECK(probe->wb_x > 0.0f);
    CHECK(probe->wc_z < 0.0f);
    CHECK(probe->all == doctest::Approx(9.5f).epsilon(0.01));    // a at z=0
    CHECK(probe->skip == doctest::Approx(14.5f).epsilon(0.01));  // b at z=5 (sensor skipped)
    CHECK(probe->sensor == doctest::Approx(11.5f).epsilon(0.01)); // trigger at z=2
}
