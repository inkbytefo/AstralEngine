#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace astral;

namespace {

struct Spin {
    float speed = 1.0f;
};

struct Recorder {
    std::vector<std::string> events;
};

class RecordingSystem final : public System {
public:
    RecordingSystem(std::string name, Recorder& rec) : name_(std::move(name)), rec_(rec) {}
    void update(Context&) override { rec_.events.push_back(name_); }

private:
    std::string name_;
    Recorder& rec_;
};

class SpinSystem final : public System {
public:
    void update(Context& ctx) override {
        ctx.world.each<Transform, const Spin>([&](Transform& t, const Spin& s) {
            t.rotation = Quat::from_axis_angle({0, 1, 0}, s.speed * ctx.time.delta) * t.rotation;
            t.position.x += 1.0f;
        });
    }
};

class TestGame final : public Game {
public:
    Recorder rec;
    int starts = 0, updates = 0, shutdowns = 0;
    Entity cube;

    void on_start(Context& ctx) override {
        ++starts;
        cube = ctx.world.create(Transform{}, Spin{2.0f});
        ctx.add_system<RecordingSystem>(Phase::PreUpdate, "pre", rec);
        ctx.add_system<RecordingSystem>(Phase::Update, "update", rec);
        ctx.add_system<SpinSystem>(Phase::Update);
        ctx.add_system<RecordingSystem>(Phase::PostUpdate, "post", rec);
    }
    void on_update(Context&) override {
        ++updates;
        rec.events.push_back("game");
    }
    void on_shutdown(Context&) override { ++shutdowns; }
};

AppDesc headless() { return {.headless = true, .fixed_timestep = 1.0f / 60.0f}; }

} // namespace

TEST_CASE("headless app runs the game lifecycle for N frames") {
    App app(headless());
    TestGame game;
    app.run_frames(game, 3);
    CHECK(game.starts == 1);
    CHECK(game.updates == 3);
    CHECK(game.shutdowns == 1);
}

TEST_CASE("phases run in order every frame") {
    App app(headless());
    TestGame game;
    app.run_frames(game, 2);
    const std::vector<std::string> expected{"pre", "game", "update", "post",
                                            "pre", "game", "update", "post"};
    CHECK(game.rec.events == expected);
}

TEST_CASE("systems change the world and transforms are propagated each frame") {
    App app(headless());
    TestGame game;
    app.run_frames(game, 4);
    const WorldTransform* w = app.world().get<WorldTransform>(game.cube);
    REQUIRE(w != nullptr);
    CHECK(w->matrix[3].x == doctest::Approx(4.0f));
}

TEST_CASE("time advances with the fixed timestep") {
    App app(headless());
    TestGame game;
    app.run_frames(game, 60);
    CHECK(app.time().frame == 60);
    CHECK(app.time().elapsed == doctest::Approx(1.0));
}

TEST_CASE("quit stops a running app") {
    class QuitGame final : public Game {
    public:
        int frames = 0;
        void on_update(Context& ctx) override {
            if (++frames == 5) ctx.quit();
        }
    };
    App app(headless());
    QuitGame game;
    CHECK(app.run(game) == 0);
    CHECK(game.frames == 5);
}

TEST_CASE("commands recorded by systems are applied within the frame") {
    class Spawner final : public System {
    public:
        void update(Context& ctx) override {
            ctx.world.each<const Spin>([&](Entity, const Spin&) { ctx.world.commands().spawn(Transform{}); });
        }
    };
    class SpawnGame final : public Game {
    public:
        void on_start(Context& ctx) override {
            ctx.world.create(Spin{});
            ctx.add_system<Spawner>(Phase::Update);
        }
    };
    App app(headless());
    SpawnGame game;
    app.run_frames(game, 1);
    CHECK(app.world().count<Transform>() == 1);
}

TEST_CASE("headless app has no window or renderer but a working world") {
    App app(headless());
    CHECK(app.context().window == nullptr);
    CHECK(app.context().renderer == nullptr);
    CHECK(app.context().world.count() == 0);
}
