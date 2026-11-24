#include <astral/astral.hpp>

#include <cmath>
#include <cstdlib>
#include <string>

namespace {
class VfxLab final : public astral::Game {
public:
    VfxLab(float hour, float speed, bool baseline, bool profile, bool aerial, float fog, bool smoke, bool vfx)
        : hour_(hour), speed_(speed), baseline_(baseline), profile_(profile), aerial_(aerial), fog_(fog), smoke_(smoke), vfx_(vfx) {}

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
        if (smoke_ || vfx_) {
            std::vector<std::pair<astral::Entity, astral::Vec3>> colliders;
            ctx.world.each<const astral::PrimitiveMesh, const astral::Transform>(
                [&](astral::Entity e, const astral::PrimitiveMesh& mesh, const astral::Transform& t) {
                    if (mesh.shape == astral::Primitive::Cube) colliders.emplace_back(e, t.scale * 0.5f);
                });
            for (const auto& [e, half] : colliders) {
                ctx.world.add(e, astral::RigidBody{.desc = {.shape = astral::ShapeDesc::box(half),
                                                          .motion = astral::MotionType::Static}});
            }
            if (vfx_) astral::play_vfx(ctx, "courtyard.vfx", {-2.8f, 1.05f, -2.4f});
            else ctx.world.create(astral::Transform{.position = {-2.8f, 1.05f, -2.4f}},
                astral::GameplaySmoke({.radius_cells = 5, .cell_size_m = 0.4f, .extinction_per_m = 1.7f,
                                       .spread_interval_steps = 1}));
        }
        ctx.world.create(astral::Transform{.position = {0, 3.5f, 11.0f},
                                           .rotation = astral::Quat::from_axis_angle({1, 0, 0}, astral::radians(-8.0f))},
                         astral::Camera{.fov_y = astral::radians(58.0f)});
    }

    void on_shutdown(astral::Context& ctx) override {
        if (!profile_ || !ctx.renderer) return;
        const auto stats = ctx.renderer->stats();
        ctx.log.info("vfx_lab volume bricks: {}, budget fallbacks: {}", stats.volume_bricks, stats.volume_brick_fallbacks);
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
    bool smoke_;
    bool vfx_;
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
                options.has("--baseline"), options.has("--profile"), !options.has("--no-aerial"), number_or(options.value("--fog", "0"), 0), options.has("--smoke"), options.has("--vfx"));
    return astral::run_game(options, {.title = "Astral VFX Lab",
                                     .render = {.ray_tracing = rt, .path_tracer = options.has("--path-tracer"),
                                                .fog_self_shadows = !options.has("--no-volume-shadows"),
                                                .fog_ddgi = !options.has("--no-fog-ddgi")},
                                     .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 8);
}
