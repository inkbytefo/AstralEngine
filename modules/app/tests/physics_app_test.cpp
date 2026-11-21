#include <astral/astral.hpp>

#include <functional>
#include <doctest/doctest.h>

using namespace astral;

namespace {
AppDesc headless(float dt = 1.0f / 60.0f) { return {.headless = true, .fixed_timestep = dt}; }

Entity make_ground(World& w) {
    return w.create(Transform{.position = {0, -0.5f, 0}},
                    RigidBody{.desc = {.shape = ShapeDesc::box({50, 0.5f, 50}), .motion = MotionType::Static}});
}

class Scene final : public Game {
public:
    std::function<void(Context&)> start;
    void on_start(Context& ctx) override { start(ctx); }
};
} // namespace

TEST_CASE("an entity with a rigid body falls and lands on static ground") {
    App app(headless());
    Entity ball{};
    Scene game;
    game.start = [&](Context& ctx) {
        make_ground(ctx.world);
        ball = ctx.world.create(Transform{.position = {0, 5, 0}},
                                RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
    };
    app.run_frames(game, 240);
    CHECK(app.world().get<Transform>(ball)->position.y == doctest::Approx(0.5f).epsilon(0.05));
    CHECK(app.context().physics->body_count() == 2);
}

TEST_CASE("bodies of removed entities are destroyed") {
    App app(headless());
    Entity a{}, b{};
    class Remover final : public System {
    public:
        Entity* a;
        Entity* b;
        int frame = 0;
        void update(Context& ctx) override {
            if (++frame == 3) {
                ctx.world.destroy(*a);
                ctx.world.remove<RigidBody>(*b);
            }
        }
    };
    Scene game;
    game.start = [&](Context& ctx) {
        a = ctx.world.create(Transform{}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
        b = ctx.world.create(Transform{}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
        auto& r = ctx.add_system<Remover>(Phase::Update);
        r.a = &a;
        r.b = &b;
    };
    app.run_frames(game, 5);
    CHECK(app.context().physics->body_count() == 0);
}

TEST_CASE("fixed update runs at 60 Hz regardless of frame rate") {
    class Counter final : public System {
    public:
        int* count;
        void update(Context& ctx) override {
            ++*count;
            CHECK(ctx.time.fixed_delta == doctest::Approx(1.0f / 60.0f));
        }
    };
    int count = 0;
    App app(headless(1.0f / 30.0f)); // 30 FPS frames
    Scene game;
    game.start = [&](Context& ctx) { ctx.add_system<Counter>(Phase::FixedUpdate).count = &count; };
    app.run_frames(game, 30); // one simulated second
    CHECK(count == 60);
}

TEST_CASE("a very slow frame runs a bounded number of physics steps") {
    int count = 0;
    class Counter final : public System {
    public:
        int* count;
        void update(Context&) override { ++*count; }
    };
    App app(headless(1.0f)); // a one-second frame
    Scene game;
    game.start = [&](Context& ctx) { ctx.add_system<Counter>(Phase::FixedUpdate).count = &count; };
    app.run_frames(game, 1);
    CHECK(count <= 8); // no spiral of death
}

TEST_CASE("raycasts resolve to entities") {
    App app(headless());
    Entity ground{};
    Scene game;
    game.start = [&](Context& ctx) { ground = make_ground(ctx.world); };
    app.run_frames(game, 1);
    const auto hit = app.context().physics->raycast({0, 10, 0}, {0, -1, 0}, 100);
    REQUIRE(hit.has_value());
    CHECK(entity_of(*hit) == ground);
}

TEST_CASE("a character controller walks where the game tells it to") {
    App app(headless());
    Entity hero{};
    class Walk final : public System {
    public:
        void update(Context& ctx) override {
            ctx.world.each<CharacterController>([](CharacterController& c) { c.move = {3, 0, 0}; });
        }
    };
    Scene game;
    game.start = [&](Context& ctx) {
        make_ground(ctx.world);
        hero = ctx.world.create(Transform{.position = {0, 0.1f, 0}}, CharacterController{});
        ctx.add_system<Walk>(Phase::Update);
    };
    app.run_frames(game, 60);
    const Transform* t = app.world().get<Transform>(hero);
    CHECK(t->position.x == doctest::Approx(3.0f).epsilon(0.1));
    CHECK(app.world().get<CharacterController>(hero)->on_ground);
}

TEST_CASE("contacts of the frame are available to systems") {
    bool seen = false;
    class Watch final : public System {
    public:
        bool* seen;
        void update(Context& ctx) override {
            for (const ContactEvent& c : ctx.contacts()) {
                *seen |= c.type == ContactEvent::Type::Begin;
            }
        }
    };
    App app(headless());
    Scene game;
    game.start = [&](Context& ctx) {
        make_ground(ctx.world);
        ctx.world.create(Transform{.position = {0, 1, 0}}, RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
        ctx.add_system<Watch>(Phase::PostUpdate).seen = &seen;
    };
    app.run_frames(game, 60);
    CHECK(seen);
}
