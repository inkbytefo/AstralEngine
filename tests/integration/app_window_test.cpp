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
