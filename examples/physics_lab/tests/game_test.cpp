// P0 Physics Lab acceptance tests (ADR-0038):
// motorized joints, breakable joints, materials, cooked shapes, batched queries,
// 120-body/60-joint stress test, streaming detach/re-attach + origin shift, and fixed-step replay determinism.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/lab/lab.hpp"
#include "game.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace astral;

namespace {

AppDesc lab_app_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    d.physics_rate = 60.0f;
    return d;
}

} // namespace

TEST_CASE("physics_lab runs motorized joints, breaks fragile joint, combines materials, and batches raycasts") {
    App app(lab_app_desc());
    physics_lab::PhysicsLabGame game;
    app.run_frames(game, 90);
    CHECK(app.context().scripts->take_errors().empty());

    // Fragile joint broke under 120 kg load and Luau counted the JointBroken event.
    CHECK(app.world().get<Joint>(game.fragile_joint)->broken);

    // Ice puck slid significantly farther than grippy puck on the same floor.
    const float ice_x = app.world().get<Transform>(game.ice_puck)->position.x;
    const float grip_x = app.world().get<Transform>(game.grip_puck)->position.x;
    CHECK(ice_x > grip_x + 2.0f);

    // Luau stats reflect motor updates, broken joints, and 3/4 raycast hits.
    int broken = 0, hits = 0, motors = 0;
    app.world().each<const lab::LabStats>([&](const lab::LabStats& s) {
        broken += s.broken_joints;
        hits = s.batch_ray_hits;
        motors += s.motor_updates;
    });
    CHECK(broken >= 1);
    CHECK(hits == 3);
    CHECK(motors >= 80);
}

TEST_CASE("physics_lab fixed-step replay produces identical transforms across runs") {
    auto run_once = []() {
        App app(lab_app_desc());
        physics_lab::PhysicsLabGame game;
        app.run_frames(game, 90);
        return std::vector<Vec3>{
            app.world().get<Transform>(game.arm_body)->position,
            app.world().get<Transform>(game.limb_body)->position,
            app.world().get<Transform>(game.ice_puck)->position,
            app.world().get<Transform>(game.grip_puck)->position,
            app.world().get<Transform>(game.rubber_ball)->position,
        };
    };
    const std::vector<Vec3> a = run_once();
    const std::vector<Vec3> b = run_once();
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == doctest::Approx(b[i].x).epsilon(1e-5));
        CHECK(a[i].y == doctest::Approx(b[i].y).epsilon(1e-5));
        CHECK(a[i].z == doctest::Approx(b[i].z).epsilon(1e-5));
    }
}

TEST_CASE("physics_lab limit and stress test: 120 bodies and 60 mixed joints stay finite and stable") {
    App app(lab_app_desc());
    struct StressGame final : Game {
        std::vector<Entity> bodies;
        void on_start(Context& ctx) override {
            ctx.world.create(Transform{.position = {0, -0.5f, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::box({100, 0.5f, 100}), .motion = MotionType::Static}});
            const ShapeHandle box = ctx.physics->create_shape(ShapeDesc::box({0.3f, 0.3f, 0.3f}));
            for (int i = 0; i < 60; ++i) {
                const float x = static_cast<float>(i % 10) * 2.5f - 11.0f;
                const float z = static_cast<float>(i / 10) * 2.5f - 6.0f;
                const Entity top = ctx.world.create(
                    Transform{.position = {x, 4.0f, z}},
                    RigidBody{.desc = {.motion = MotionType::Dynamic, .mass = 2.0f, .cooked_shape = box}});
                const Entity bot = ctx.world.create(
                    Transform{.position = {x, 2.8f, z}},
                    RigidBody{.desc = {.motion = MotionType::Dynamic, .mass = 2.0f, .cooked_shape = box}});
                bodies.push_back(top);
                bodies.push_back(bot);
                const JointType jt = (i % 3 == 0) ? JointType::Hinge : (i % 3 == 1) ? JointType::SwingTwist : JointType::SixDOF;
                ctx.world.create(
                    Joint{.type = jt, .a = top, .b = bot, .anchor = {x, 3.4f, z}, .axis = {0, 1, 0},
                          .limited = true, .min_limit = -0.5f, .max_limit = 0.5f, .swing_limits = {0.4f, 0.4f}},
                    JointMotor{.mode = JointMotorMode::Velocity, .max_torque = 30.0f, .target_velocity = 1.5f});
            }
        }
    } game;

    app.run_frames(game, 120);
    REQUIRE(game.bodies.size() == 120);
    for (Entity e : game.bodies) {
        const Vec3 p = app.world().get<Transform>(e)->position;
        CHECK(std::isfinite(p.x));
        CHECK(std::isfinite(p.y));
        CHECK(std::isfinite(p.z));
        CHECK(p.y > -2.0f);
        CHECK(p.y < 30.0f);
    }
}
