#include <astral/astral.hpp>
#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("windowed app renders a scene of meshes") {
    class Scene final : public Game {
    public:
        void on_start(Context& ctx) override {
            if (!ctx.renderer) return;
            for (int i = 0; i < 50; ++i) {
                ctx.world.create(Transform{.position = {static_cast<float>(i % 10) - 5, 0, -static_cast<float>(i / 10)}},
                                 MeshRenderer{ctx.meshes.cube, {0.8f, 0.3f, 0.2f, 1}});
            }
            ctx.world.create(Transform{.position = {0, 3, 8}}, Camera{});
        }
    };
    App app({.title = "app test", .width = 320, .height = 240, .validation = true});
    if (!app.context().renderer) {
        MESSAGE("no window/GPU — skipping");
        return;
    }
    Scene scene;
    app.run_frames(scene, 10);
    CHECK(app.context().renderer->stats().instances == 50);
}

TEST_CASE("systems can release GPU resources in their destructors at shutdown") {
    class OwnsMesh final : public System {
    public:
        explicit OwnsMesh(Context& ctx) : renderer_(ctx.renderer), mesh_(ctx.renderer->create_mesh(make_cube())) {}
        ~OwnsMesh() override { renderer_->destroy(mesh_); }
        void update(Context&) override {}

    private:
        Renderer* renderer_;
        MeshHandle mesh_;
    };
    class Owner final : public Game {
    public:
        void on_start(Context& ctx) override {
            if (ctx.renderer) ctx.add_system<OwnsMesh>(Phase::Update, ctx);
        }
    };
    {
        App app({.title = "shutdown", .width = 64, .height = 64});
        if (!app.context().renderer) return;
        Owner game;
        app.run_frames(game, 2);
    } // ~App must destroy the system before the renderer
    CHECK(true);
}
