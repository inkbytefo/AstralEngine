// Contract tests for F1 PhysicsService queued commands, fixed-step timing, parallel determinism,
// and direct PhysicsWorld C++ consumer independence (ADR-0034).
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

struct TagA { int idx = 0; };
struct TagB { int idx = 0; };

class ImpulseSystemA final : public System {
public:
    void configure(SystemAccess& access) override { access.reads<TagA>(); }
    void update(Context& ctx) override {
        ctx.world.each<const TagA>([&](Entity e, const TagA& t) {
            ctx.physics_service().add_impulse(e, {static_cast<float>(t.idx + 1) * 0.5f, 3.0f, 0.0f});
        });
    }
};

class ImpulseSystemB final : public System {
public:
    void configure(SystemAccess& access) override { access.reads<TagB>(); }
    void update(Context& ctx) override {
        ctx.world.each<const TagB>([&](Entity e, const TagB& t) {
            ctx.physics_service().add_force(e, {0.0f, 0.0f, static_cast<float>(t.idx + 1) * 15.0f});
        });
    }
};

} // namespace

TEST_CASE("zero-substep frame retains queued commands until the next fixed step executes") {
    struct G final : Game {
        Entity ball{};
        bool queued = false;
        std::size_t pending_after_frame1 = 0;
        std::uint64_t step_after_frame1 = 99;
        float vy_after_frame1 = 99.0f;
        float vy_after_step = 0.0f;

        void on_start(Context& ctx) override {
            ball = ctx.world.create(Transform{.position = {0, 5, 0}},
                                    RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 1.0f}});
        }
        void on_update(Context& ctx) override {
            if (!queued) {
                ctx.physics_service().set_linear_velocity(ball, {0.0f, 12.0f, 0.0f});
                queued = true;
            }
            if (ctx.time.frame == 1) {
                pending_after_frame1 = ctx.physics_service().pending_command_count();
                step_after_frame1 = ctx.physics_service().query_state().step_index;
                vy_after_frame1 = ctx.physics_service().linear_velocity(ball).y;
            }
        }
        void on_shutdown(Context& ctx) override {
            vy_after_step = ctx.physics_service().linear_velocity(ball).y;
        }
    } game;

    // fixed_delta = 1/60 (~0.01667s), frame delta = 0.010s -> frame 0 runs 0 steps, frame 1 runs 1 step.
    App app({.headless = true, .fixed_timestep = 0.010f, .physics_rate = 60.0f});
    app.run_frames(game, 2);
    CHECK(game.step_after_frame1 == 0);
    CHECK(game.pending_after_frame1 == 1);
    CHECK(game.vy_after_frame1 == doctest::Approx(0.0f));
    CHECK(app.context().physics_service().query_state().step_index == 1);
    CHECK(game.vy_after_step > 10.0f);
}

TEST_CASE("delayed spawn and same-tick Luau/C++ physics commands apply after body sync") {
    const auto dir = test::temp_dir("f1_delayed_spawn_cmd");
    write_text(dir / "launch.luau", R"(
        local launched = false
        astral.system {
            phase = "FixedUpdate",
            run = function()
                if launched then return end
                launched = true
                local target = astral.find("late_body")
                if target then
                    astral.physics.impulse(target, vector.create(0, 14, 0))
                end
            end
        }
    )");

    struct G final : Game {
        Entity late{};
        Entity dead{};
        bool spawned = false;
        void on_start(Context& ctx) override {
            REQUIRE(ctx.script_service().load_script("launch.luau").empty());
            dead = ctx.world.create(Transform{.position = {5, 5, 0}},
                                    RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 1.0f}});
            ctx.world.destroy(dead);
        }
        void on_update(Context& ctx) override {
            if (spawned) return;
            spawned = true;
            // Deferred component add + same-frame command on newly spawned body + dead entity command.
            late = ctx.world.create(Name{"late_body"}, Transform{.position = {0, 3, 0}});
            ctx.world.commands().add(late, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 1.0f}});
            ctx.physics_service().add_impulse(dead, {100.0f, 0.0f, 0.0f}); // dead entity: must be ignored safely
        }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 2);
    CHECK(app.context().scripts->take_errors().empty());
    const Vec3 vel = app.context().physics_service().linear_velocity(game.late);
    CHECK(vel.y > 10.0f);
    CHECK(app.context().physics_service().linear_velocity(game.dead).x == 0.0f);
}

TEST_CASE("multi-substep frames advance step_index accurately and cap backlog at max_steps") {
    struct G final : Game {
        std::vector<std::uint64_t> fixed_steps_seen;
        void on_start(Context& ctx) override {
            class StepProbe final : public System {
            public:
                explicit StepProbe(std::vector<std::uint64_t>& out) : out_(out) {}
                void update(Context& ctx) override {
                    const PhysicsQueryState qs = ctx.physics_service().query_state();
                    CHECK(qs.in_fixed_update);
                    out_.push_back(qs.step_index);
                }
            private:
                std::vector<std::uint64_t>& out_;
            };
            ctx.add_system<StepProbe>(Phase::FixedUpdate, fixed_steps_seen);
        }
    } game;

    // 60 Hz physics, max_physics_steps = 3, frame delta = 0.10 s (6 steps worth) -> capped at 3 steps.
    App app({.headless = true, .fixed_timestep = 0.10f, .physics_rate = 60.0f, .max_physics_steps = 3});
    app.run_frames(game, 1);
    REQUIRE(game.fixed_steps_seen.size() == 3);
    CHECK(game.fixed_steps_seen[0] == 0);
    CHECK(game.fixed_steps_seen[1] == 1);
    CHECK(game.fixed_steps_seen[2] == 2);
    const PhysicsQueryState end_state = app.context().physics_service().query_state();
    CHECK(end_state.step_index == 3);
    CHECK_FALSE(end_state.in_fixed_update);
}

TEST_CASE("parallel and serial systems queueing physics commands produce identical simulation state") {
    auto run_sim = [](bool parallel) {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = 60.0f, .parallel_systems = parallel});
        struct SimGame final : Game {
            std::vector<Entity> bodies;
            void on_start(Context& ctx) override {
                for (int i = 0; i < 16; ++i) {
                    const Entity e = ctx.world.create(
                        Transform{.position = {static_cast<float>(i) * 3.0f, 4.0f, 0.0f}},
                        RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 1.0f}},
                        TagA{i}, TagB{i});
                    bodies.push_back(e);
                }
                ctx.add_system<ImpulseSystemA>(Phase::FixedUpdate);
                ctx.add_system<ImpulseSystemB>(Phase::FixedUpdate);
            }
        } game;
        app.run_frames(game, 5);
        std::vector<Vec3> positions;
        for (Entity e : game.bodies) positions.push_back(app.world().get<const Transform>(e)->position);
        return positions;
    };

    const std::vector<Vec3> serial = run_sim(false);
    const std::vector<Vec3> parallel = run_sim(true);
    REQUIRE(serial.size() == parallel.size());
    for (std::size_t i = 0; i < serial.size(); ++i) {
        CHECK(serial[i].x == parallel[i].x);
        CHECK(serial[i].y == parallel[i].y);
        CHECK(serial[i].z == parallel[i].z);
    }
}

TEST_CASE("direct PhysicsWorld C++ consumers retain independent manual step control") {
    Logger log;
    PhysicsWorld physics({.logger = &log});
    const BodyHandle floor = physics.create_body({.shape = ShapeDesc::box({10, 0.5f, 10}), .motion = MotionType::Static});
    BodyDesc ball_desc;
    ball_desc.shape = ShapeDesc::sphere(0.5f);
    ball_desc.mass = 1.0f;
    ball_desc.position = {0, 1.5f, 0};
    const BodyHandle ball = physics.create_body(ball_desc);
    REQUIRE(floor.valid());
    REQUIRE(ball.valid());

    physics.add_impulse(ball, {2.0f, 0.0f, 0.0f});
    physics.step(1.0f / 60.0f);
    CHECK(physics.position(ball).x > 0.0f);
    CHECK(physics.linear_velocity(ball).x == doctest::Approx(2.0f).epsilon(0.05));
}
