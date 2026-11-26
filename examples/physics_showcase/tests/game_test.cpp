// physics_showcase, headless. The checks of the three examples it replaces (physics_demo, physics_lab,
// physics_playground), the newer Luau physics calls, and a scripted-input run of the player.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/fly_camera/fly_camera.hpp"
#include "features/lab/lab.hpp"
#include "features/lifetime/lifetime.hpp"
#include "features/player/player.hpp"
#include "features/playground/playground.hpp"
#include "features/showcase/showcase.hpp"
#include "game.hpp"
#include "stations/stations.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

using namespace astral;
using physics_showcase::ShowcaseGame;

namespace {

AppDesc showcase_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    d.physics_rate = 60.0f;
    return d;
}

} // namespace

TEST_CASE("water and wind: crates float, stones sink, the bridge holds, the wind turns, the pad throws") {
    App app(showcase_desc());
    ShowcaseGame game;
    app.run_frames(game, 60 * 8);
    World& w = app.world();
    CHECK(app.context().scripts->take_errors().empty());

    float crate_lowest = 100.0f, crate_highest = -100.0f, stone_highest = -100.0f;
    std::size_t crates = 0, stones = 0;
    w.each<const RigidBody, const Transform, const Buoyant>([&](const RigidBody& rb, const Transform& t, const Buoyant& b) {
        if (rb.desc.shape.type != ShapeType::Box || b.volume < 0.2f) return;
        if (rb.desc.mass == 90.0f && t.position.z < 0) { // the six crates on the lake (dropped ones start at z = 0)
            ++crates;
            crate_lowest = std::min(crate_lowest, t.position.y);
            crate_highest = std::max(crate_highest, t.position.y);
        } else if (rb.desc.mass == 420.0f) {
            ++stones;
            stone_highest = std::max(stone_highest, t.position.y);
        }
    });
    CHECK(crates == 6);
    CHECK(crate_lowest > -0.6f); // floating at the surface (y = 0), not on the bottom
    CHECK(crate_highest < 0.6f);
    CHECK(stones == 3);
    CHECK(stone_highest < -1.0f); // sunk

    float lowest_plank = 100.0f;
    std::size_t planks = 0;
    w.each<const RigidBody, const Transform>([&](const RigidBody& rb, const Transform& t) {
        if (rb.desc.mass == 20.0f) {
            ++planks;
            lowest_plank = std::min(lowest_plank, t.position.y);
        }
    });
    CHECK(planks == 14);
    CHECK(lowest_plank > physics_showcase::terrain_height(-14.0f, 17.0f) - 1.5f); // sags, does not fall

    Vec3 wind{};
    w.each<const WindZone>([&](const WindZone& z) { wind = z.velocity; });
    CHECK(length(wind) > 1.0f);

    const playground::ContactStats* stats = w.get<const playground::ContactStats>(find_by_name(w, "playground_stats"));
    REQUIRE(stats != nullptr);
    CHECK(stats->begin_count > 0);
    CHECK(stats->pad_launches > 0);
}

TEST_CASE("joints: Luau drives the motor, the fragile joint breaks, the batched sensor array sees 3 of 4") {
    App app(showcase_desc());
    ShowcaseGame game;
    app.run_frames(game, 90);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().get<Joint>(game.handles.fragile_joint)->broken);
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

TEST_CASE("surfaces: ice slides farther than grip; torque spins the wheel; the scanner ignores itself and sees the sensor") {
    App app(showcase_desc());
    ShowcaseGame game;
    app.run_frames(game, 120);
    CHECK(app.context().scripts->take_errors().empty());
    World& w = app.world();
    // Both pucks start at the same x moving toward -x at 6 m/s.
    CHECK(w.get<Transform>(game.handles.ice_puck)->position.x < w.get<Transform>(game.handles.grip_puck)->position.x - 2.0f);

    const auto* spinner = w.get<const showcase::Spinner>(game.handles.wheel);
    REQUIRE(spinner);
    CHECK(spinner->speed > 2.0f); // astral.physics.torque + angular_velocity
    CHECK(length(app.context().physics_service().angular_velocity(game.handles.wheel)) > 2.0f);

    const auto* scanner = w.get<const showcase::Scanner>(game.handles.scanner);
    REQUIRE(scanner);
    CHECK(scanner->hit == game.handles.scanner_gate); // not its own collider (ignore), and the gate is a sensor (sensors = true)
    CHECK(scanner->distance == doctest::Approx(4.9f).epsilon(0.05));
    CHECK(scanner->hits > 100);
}

TEST_CASE("pile: balls rain and expire; the player stands on the plateau") {
    App app(showcase_desc());
    ShowcaseGame game;
    app.run_frames(game, 180);
    World& w = app.world();
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(w.count<lifetime::Lifetime>() >= 4);
    const float y = w.get<const Transform>(game.handles.player)->position.y;
    CHECK(y > physics_showcase::plateau_height - 0.5f);
    CHECK(y < physics_showcase::plateau_height + 1.0f);
}

TEST_CASE("scripted input: the player walks, jumps, and F hands the camera to free flight") {
    const std::vector<InputScriptEvent> script{
        {.frame = 10, .key = Key::W, .down = true},       {.frame = 100, .key = Key::W, .down = false},
        {.frame = 120, .key = Key::Space, .down = true},  {.frame = 122, .key = Key::Space, .down = false},
        {.frame = 180, .key = Key::F, .down = true},      {.frame = 182, .key = Key::F, .down = false},
    };
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    struct Recorder final : Game {
        ShowcaseGame inner;
        Vec3 start{};
        float peak_y = -1e9f;
        void on_start(Context& ctx) override {
            inner.on_start(ctx);
            start = ctx.world.get<const Transform>(inner.handles.player)->position;
        }
        void on_update(Context& ctx) override {
            inner.on_update(ctx);
            if (ctx.time.frame >= 120 && ctx.time.frame <= 170) {
                peak_y = std::max(peak_y, ctx.world.get<const Transform>(inner.handles.player)->position.y);
            }
        }
    } game;
    app.run_frames(game, 200);
    CHECK(app.context().scripts->take_errors().empty());
    World& w = app.world();
    const Vec3 end = w.get<const Transform>(game.inner.handles.player)->position;
    CHECK(end.z < game.start.z - 5.0f);         // walked forward (toward -z, away from the camera)
    CHECK(game.peak_y > game.start.y + 0.5f);   // jumped
    const auto* cam = w.get<const player::PlayerCamera>(game.inner.handles.camera);
    REQUIRE(cam);
    CHECK(cam->flying);
    CHECK(w.has<fly::FlyCamera>(game.inner.handles.camera));
}

TEST_CASE("the scene-authored part behaves the same from loose assets and the package") {
    const auto run = [](bool packed) {
        AppDesc desc = showcase_desc();
        if (packed) {
            desc.asset_root = {};
            desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
            REQUIRE(std::filesystem::is_regular_file(desc.pack));
        }
        App app(desc);
        ShowcaseGame game;
        app.run_frames(game, 180);
        CHECK(app.context().scripts->take_errors().empty());
        World& world = app.world();
        const Entity character = find_by_name(world, "scene_character");
        const auto* controller = world.get<const CharacterController>(character);
        const auto* sensor_body = world.get<const RigidBody>(find_by_name(world, "scene_sensor"));
        const auto* slider = world.get<const Joint>(find_by_name(world, "scene_joint"));
        REQUIRE(controller);
        REQUIRE(sensor_body);
        REQUIRE(slider);
        CHECK(controller->on_ground);
        CHECK(sensor_body->desc.sensor);
        CHECK(app.context().physics->valid(slider->handle));
        const Vec3 feet = world.get<const Transform>(character)->position;
        CHECK(feet.x > 0.8f); // the Luau driver walked it across the sensor
        return feet;
    };
    const Vec3 loose = run(false);
    const Vec3 packed = run(true);
    CHECK(loose.x == doctest::Approx(packed.x));
    CHECK(loose.y == doctest::Approx(packed.y));
    CHECK(loose.z == doctest::Approx(packed.z));
}

TEST_CASE("fixed-step replay produces identical transforms across runs") {
    auto run_once = []() {
        App app(showcase_desc());
        ShowcaseGame game;
        app.run_frames(game, 90);
        const auto at = [&](Entity e) { return app.world().get<Transform>(e)->position; };
        return std::vector<Vec3>{at(game.handles.arm_body), at(game.handles.limb_body), at(game.handles.ice_puck),
                                 at(game.handles.grip_puck), at(game.handles.rubber_ball), at(game.handles.player)};
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

TEST_CASE("stress: 120 bodies and 60 mixed motorized joints stay finite and stable") {
    App app(showcase_desc());
    struct StressGame final : Game {
        std::vector<Entity> bodies;
        void on_start(Context& ctx) override {
            ctx.world.create(Transform{.position = {0, -0.5f, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::box({100, 0.5f, 100}), .motion = MotionType::Static}});
            const ShapeHandle box = ctx.physics->create_shape(ShapeDesc::box({0.3f, 0.3f, 0.3f}));
            for (int i = 0; i < 60; ++i) {
                const float x = static_cast<float>(i % 10) * 2.5f - 11.0f;
                const float z = static_cast<float>(i / 10) * 2.5f - 6.0f;
                const Entity top = ctx.world.create(Transform{.position = {x, 4.0f, z}},
                                                    RigidBody{.desc = {.motion = MotionType::Dynamic, .mass = 2.0f, .cooked_shape = box}});
                const Entity bot = ctx.world.create(Transform{.position = {x, 2.8f, z}},
                                                    RigidBody{.desc = {.motion = MotionType::Dynamic, .mass = 2.0f, .cooked_shape = box}});
                bodies.push_back(top);
                bodies.push_back(bot);
                const JointType jt = (i % 3 == 0) ? JointType::Hinge : (i % 3 == 1) ? JointType::SwingTwist : JointType::SixDOF;
                ctx.world.create(Joint{.type = jt, .a = top, .b = bot, .anchor = {x, 3.4f, z}, .axis = {0, 1, 0},
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
