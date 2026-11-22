// Fields, forces, physics events and interpolation inside the App (M12.5).
#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <functional>

using namespace astral;

namespace {

App make_app(bool interpolation = false, float physics_rate = 60.0f) {
    return App({.headless = true, .fixed_timestep = 1.0f / 60.0f, .physics_rate = physics_rate, .hot_reload = false, .physics_interpolation = interpolation});
}

struct Scene final : Game {
    std::function<void(Context&)> setup, each_frame;
    void on_start(Context& ctx) override {
        if (setup) setup(ctx);
    }
    void on_update(Context& ctx) override {
        if (each_frame) each_frame(ctx);
    }
};

Entity ground(World& w) {
    return w.create(Transform{.position = {0, -0.5f, 0}}, RigidBody{.desc = {.shape = ShapeDesc::box({100, 0.5f, 100}), .motion = MotionType::Static, .surface = 3}});
}

} // namespace

TEST_CASE("wind pushes a body that has air drag; a wind zone is a box") {
    App app = make_app();
    Scene scene;
    Entity inside{}, outside{};
    scene.setup = [&](Context& ctx) {
        ctx.world.create(Transform{.position = {0, 5, 0}}, WindZone{.half_extents = {10, 10, 10}, .velocity = {12, 0, 0}});
        inside = ctx.world.create(Transform{.position = {0, 5, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 5.0f, .linear_damping = 0.0f, .gravity_factor = 0.0f}},
                                  AirDrag{.area = 0.8f, .drag = 0.5f});
        outside = ctx.world.create(Transform{.position = {0, 40, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .mass = 5.0f, .linear_damping = 0.0f, .gravity_factor = 0.0f}},
                                   AirDrag{.area = 0.8f, .drag = 0.5f});
    };
    app.run_frames(scene, 120);
    World& w = app.world();
    CHECK(w.get<Transform>(inside)->position.x > 3.0f);                     // blown along +x
    CHECK(std::abs(w.get<Transform>(outside)->position.x) < 1e-3f);         // no wind up there
    CHECK(app.context().fields.wind_at({0, 5, 0}, 0.0).x == doctest::Approx(12.0f));
    CHECK(app.context().fields.wind_at({0, 40, 0}, 0.0).x == 0.0f);
}

TEST_CASE("a light body floats at the surface, a heavy one sinks; water drag slows a swimmer") {
    App app = make_app();
    Scene scene;
    Entity floater{}, stone{}, swimmer{};
    scene.setup = [&](Context& ctx) {
        ctx.world.create(Transform{.position = {0, -2, 0}}, WaterBody{.half_extents = {30, 2, 30}});
        // 0.6 m cube: volume 0.216 m^3. 100 kg floats (46 % under); 400 kg sinks.
        const BodyDesc cube{.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}), .linear_damping = 0.0f};
        BodyDesc light = cube, heavy = cube;
        light.mass = 100.0f;
        heavy.mass = 400.0f;
        floater = ctx.world.create(Transform{.position = {0, 1.0f, 0}}, RigidBody{.desc = light}, Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f});
        stone = ctx.world.create(Transform{.position = {5, 1.0f, 0}}, RigidBody{.desc = heavy}, Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f});
        BodyDesc fast = light;
        fast.linear_velocity = {10, 0, 0};
        fast.gravity_factor = 0.0f;
        swimmer = ctx.world.create(Transform{.position = {-10, -1.0f, 0}}, RigidBody{.desc = fast}, Buoyant{.volume = 0.0f, .height = 0.6f, .area = 0.36f, .drag = 1.0f});
    };
    app.run_frames(scene, 60 * 6);
    World& w = app.world();
    const float floater_y = w.get<Transform>(floater)->position.y;
    CHECK(floater_y > -0.15f);   // surface is at y = 0; the centre of a floating 0.6 m cube sits within a few cm of it
    CHECK(floater_y < 0.2f);
    CHECK(std::abs(w.get<RigidBody>(floater) ? app.context().physics->linear_velocity(w.get<RigidBody>(floater)->handle).y : 9.0f) < 0.3f); // settled
    CHECK(w.get<Transform>(stone)->position.y < -1.5f); // sinking
    CHECK(app.context().physics->linear_velocity(w.get<RigidBody>(swimmer)->handle).x < 5.0f); // lost speed to the water (10 -> less)
}

TEST_CASE("a current carries a floating body along") {
    App app = make_app();
    Scene scene;
    Entity boat{};
    scene.setup = [&](Context& ctx) {
        ctx.world.create(Transform{.position = {0, -2, 0}}, WaterBody{.half_extents = {200, 2, 200}, .flow = {2.0f, 0, 0}});
        boat = ctx.world.create(Transform{.position = {0, 0.0f, 0}}, RigidBody{.desc = {.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}), .mass = 100.0f}},
                                Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f, .drag = 1.2f});
    };
    app.run_frames(scene, 60 * 8);
    CHECK(app.world().get<Transform>(boat)->position.x > 3.0f);
}

TEST_CASE("a wing in moving air gets lift: a body moving forward climbs against gravity") {
    App app = make_app();
    Scene scene;
    Entity glider{}, brick{};
    scene.setup = [&](Context& ctx) {
        BodyDesc body{.shape = ShapeDesc::box({1.0f, 0.05f, 0.4f}), .linear_velocity = {0, 0, -30.0f}, .mass = 10.0f, .linear_damping = 0.0f};
        // Pitched up by 6 degrees: the nose (-z) rises, the air meets the underside.
        const Quat pitch = Quat::from_axis_angle({1, 0, 0}, radians(6.0f));
        glider = ctx.world.create(Transform{.position = {0, 50, 0}, .rotation = pitch}, RigidBody{.desc = body}, Airfoil{.area = 1.6f, .lift_slope = 6.28f});
        brick = ctx.world.create(Transform{.position = {10, 50, 0}, .rotation = pitch}, RigidBody{.desc = body});
    };
    app.run_frames(scene, 60);
    World& w = app.world();
    const float with = w.get<Transform>(glider)->position.y, without = w.get<Transform>(brick)->position.y;
    CHECK(without == doctest::Approx(50.0f - 4.9f).epsilon(0.05)); // free fall
    CHECK(with > without + 1.5f);                                   // the wing carried it
}

TEST_CASE("sensors raise trigger events, collisions raise contact events with surfaces") {
    App app = make_app();
    Scene scene;
    Entity zone{}, ball{}, floor_entity{};
    std::vector<std::string> log;
    EventReader<TriggerEntered> entered;
    EventReader<TriggerExited> exited;
    EventReader<Contact> contacts;
    int surfaces_ok = 0;
    scene.setup = [&](Context& ctx) {
        floor_entity = ground(ctx.world);
        zone = ctx.world.create(Transform{.position = {0, 3, 0}}, RigidBody{.desc = {.shape = ShapeDesc::box({2, 1, 2}), .motion = MotionType::Static, .sensor = true}});
        ball = ctx.world.create(Transform{.position = {0, 8, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.4f), .surface = 7}});
    };
    scene.each_frame = [&](Context& ctx) {
        entered.read(ctx.world, [&](const TriggerEntered& e) { if (e.trigger == zone && e.other == ball) log.push_back("enter"); });
        exited.read(ctx.world, [&](const TriggerExited& e) { if (e.trigger == zone && e.other == ball) log.push_back("exit"); });
        contacts.read(ctx.world, [&](const Contact& c) {
            if (c.kind == ContactKind::Begin && ((c.a == ball && c.b == floor_entity) || (c.a == floor_entity && c.b == ball))) {
                log.push_back("hit");
                surfaces_ok += (std::max(c.surface_a, c.surface_b) == 7 && std::min(c.surface_a, c.surface_b) == 3) ? 1 : 0;
            }
        });
    };
    app.run_frames(scene, 60 * 4);
    REQUIRE(log.size() >= 3);
    CHECK(log[0] == "enter");
    CHECK(log[1] == "exit");
    CHECK(log[2] == "hit");
    CHECK(surfaces_ok >= 1);
}

TEST_CASE("with interpolation, bodies move smoothly between slow physics steps") {
    const auto changes = [](bool interpolation) {
        App app = make_app(interpolation, 15.0f); // physics at 15 Hz, frames at 60 Hz
        Scene scene;
        Entity ball{};
        int moved = 0;
        float last = 1e9f;
        scene.setup = [&](Context& ctx) { ball = ctx.world.create(Transform{.position = {0, 100, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .linear_damping = 0.0f}}); };
        scene.each_frame = [&](Context& ctx) {
            const float y = ctx.world.get<Transform>(ball)->position.y;
            if (std::abs(y - last) > 1e-5f) ++moved;
            last = y;
        };
        app.run_frames(scene, 120);
        return moved;
    };
    const int plain = changes(false), smooth = changes(true);
    CHECK(plain < 60);   // about one change per 4 frames
    CHECK(smooth > 100); // a change nearly every frame
}

TEST_CASE("Luau sees the physics world: raycasts, impulses, wind, water and trigger events") {
    App app = make_app();
    Scene scene;
    Entity floor_entity{}, ball{};
    scene.setup = [&](Context& ctx) {
        floor_entity = ground(ctx.world);
        ctx.world.create(Name{"zone"}, Transform{.position = {5, 1, 0}}, RigidBody{.desc = {.shape = ShapeDesc::box({1, 1, 1}), .motion = MotionType::Static, .sensor = true}});
        ball = ctx.world.create(Name{"ball"}, Transform{.position = {5, 6, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f)}});
        ctx.world.create(Transform{.position = {0, 0, 0}}, WindZone{.velocity = {3, 0, 0}, .global = true});
        ctx.world.create(Transform{.position = {0, -3, 20}}, WaterBody{.half_extents = {5, 3, 5}, .density = 1010.0f});
        ctx.types.add<Contact>();
        REQUIRE(ctx.scripts->load("phys.luau", R"(
            astral.define_event("test::Report")
            local entered = 0
            local hits = 0
            astral.on("astral::TriggerEntered", function(ctx, ev)
                if ev.trigger == astral.find("zone") then entered = entered + 1 end
            end, "PostUpdate")
            astral.on("astral::Contact", function(ctx, ev)
                if ev.kind == "Begin" and ev.surface_a ~= nil then hits = hits + 1 end
            end, "PostUpdate")
            astral.system { phase = "Update", run = function(ctx)
                local down = astral.physics.raycast(vector.create(0, 5, 0), vector.create(0, -1, 0), 20)
                if not down then return end -- the bodies appear after the first physics sync
                assert(down.surface == 3, "the ground is surface 3")
                assert(astral.physics.raycast(vector.create(0, 5, 0), vector.create(0, -1, 0), 20, {layers = {5}}) == nil, "nothing is on layer 5")
                assert(astral.fields.wind(vector.create(0, 0, 0)).x == 3)
                assert(astral.fields.water(vector.create(0, -1, 20)).inside)
                assert(astral.fields.water(vector.create(0, -1, 20)).density == 1010)
                astral.send("test::Report", {entered = entered, hits = hits})
            end }
        )").empty());
    };
    int entered = 0, hits = 0;
    EventReader<DynamicEvent> reports;
    app.run_frames(scene, 60 * 3);
    reports.read(app.world(), [&](const DynamicEvent& e) {
        entered = static_cast<int>(e.payload["entered"].as_number());
        hits = static_cast<int>(e.payload["hits"].as_number());
    });
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(entered >= 1); // the ball fell through the trigger
    CHECK(hits >= 1);    // and hit the ground
}

TEST_CASE("Joint components connect rigid bodies: a pendulum swings, a hinge door turns, the joint goes with its entity") {
    App app = make_app();
    Scene scene;
    Entity bob{}, door{}, rope{};
    scene.setup = [&](Context& ctx) {
        bob = ctx.world.create(Transform{.position = {2, 5, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f), .linear_damping = 0.0f}});
        ctx.world.create(Joint{.type = JointType::Point, .b = bob, .anchor = {0, 5, 0}});
        door = ctx.world.create(Transform{.position = {0.5f, 1, 10}}, RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 1.0f, 0.05f}), .gravity_factor = 0.0f}});
        ctx.world.create(Joint{.type = JointType::Hinge, .b = door, .anchor = {0, 1, 10}, .axis = {0, 1, 0}});
        const Entity weight = ctx.world.create(Transform{.position = {20, 5, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f)}});
        rope = ctx.world.create(Joint{.type = JointType::Point, .b = weight, .anchor = {20, 6, 0}});
    };
    scene.each_frame = [&](Context& ctx) {
        if (ctx.time.frame == 10) ctx.physics->add_impulse(ctx.world.get<RigidBody>(door)->handle, {0, 0, -200.0f});
        if (ctx.time.frame == 30) ctx.world.destroy(rope);
    };
    float worst = 0.0f;
    bool door_turned = false;
    scene.each_frame = [&, previous = scene.each_frame](Context& ctx) {
        previous(ctx);
        const Vec3 p = ctx.world.get<Transform>(bob)->position;
        worst = std::max(worst, std::abs(length(p - Vec3{0, 5, 0}) - 2.0f));
        door_turned = door_turned || std::abs(ctx.world.get<Transform>(door)->rotation.y) > 0.2f;
    };
    app.run_frames(scene, 120);
    CHECK(worst < 0.1f);      // the bob stays on its circle
    CHECK(door_turned);       // the hinge door swung
    CHECK(app.context().physics->body_count() >= 2);
}
