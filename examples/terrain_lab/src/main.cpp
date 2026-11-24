#include <astral/app/shell.hpp>

namespace {
class TerrainLab final : public astral::Game {
public:
    explicit TerrainLab(bool far_view) : far_view_(far_view) {}

private:
    bool far_view_;

    void on_start(astral::Context& ctx) override {
        const auto scene = ctx.scenes().load_scene("terrain.scene");
        for (const auto& error : scene.errors) ctx.log.error("terrain lab: {}", error);
        ctx.sun = {.direction = astral::normalize(astral::Vec3{-0.5f, -0.8f, -0.3f}), .intensity = 3.0f};
        ctx.environment.ambient = 0.45f;
        const astral::Vec3 eye = far_view_ ? astral::Vec3{64, 108, 190} : astral::Vec3{64, 93, 158};
        ctx.world.create(astral::Transform{.position = eye,
                                         .rotation = astral::Quat::from_axis_angle({1, 0, 0}, astral::radians(-43.0f))},
                         astral::Camera{.fov_y = astral::radians(55.0f)});
    }
};
} // namespace

int main(int argc, char** argv) {
    const auto options = astral::parse_shell_args(argc, argv);
    TerrainLab game(options.has("--far"));
    return astral::run_game(options,
                            {.title = "Astral Terrain Lab", .render = {.ray_tracing = false},
                             .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 2);
}
