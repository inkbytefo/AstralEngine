#include <astral/astral.hpp>

#include <cmath>
#include <cstdlib>
#include <string>

namespace {
class VfxLab final : public astral::Game {
public:
    VfxLab(float hour, float speed, bool baseline, bool profile, bool aerial, float fog)
        : hour_(hour), speed_(speed), baseline_(baseline), profile_(profile), aerial_(aerial), fog_(fog) {}

private:
    void on_start(astral::Context& ctx) override {
        const auto scene = ctx.scenes().load_scene("courtyard.scene");
        if (!scene.errors.empty()) {
            ctx.quit();
            return;
        }
        if (!baseline_) ctx.environment.fog_extinction = fog_;
        if (!aerial_ || baseline_) ctx.environment.aerial_extinction = 0.0f;
        ctx.world.each<astral::Atmosphere>([&](astral::Atmosphere& sky) {
            sky.hour = hour_;
            sky.hours_per_second = speed_;
            sky.enabled = !baseline_;
        });
        ctx.world.create(astral::Transform{.position = {0, 3.5f, 11.0f},
                                           .rotation = astral::Quat::from_axis_angle({1, 0, 0}, astral::radians(-8.0f))},
                         astral::Camera{.fov_y = astral::radians(58.0f)});
    }

    void on_shutdown(astral::Context& ctx) override {
        if (!profile_ || !ctx.renderer) return;
        for (const auto& timing : ctx.renderer->gpu_timings()) {
            ctx.log.info("vfx_lab pass {}: {} ms", timing.name, timing.ms);
        }
    }

    float hour_;
    float speed_;
    bool baseline_;
    bool profile_;
    bool aerial_;
    float fog_;
};

float number_or(std::string_view text, float fallback) {
    const std::string value(text);
    char* end = nullptr;
    const float hour = std::strtof(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && std::isfinite(hour) ? hour : fallback;
}
} // namespace

int main(int argc, char** argv) {
    const auto options = astral::parse_shell_args(argc, argv);
    const bool rt = !options.has("--raster");
    VfxLab game(number_or(options.value("--hour", "12"), 12),
                number_or(options.value("--hours-per-second", "0"), 0),
                options.has("--baseline"), options.has("--profile"), !options.has("--no-aerial"), number_or(options.value("--fog", "0"), 0));
    return astral::run_game(options, {.title = "Astral VFX Lab", .render = {.ray_tracing = rt, .path_tracer = options.has("--path-tracer")},
                                     .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 8);
}
