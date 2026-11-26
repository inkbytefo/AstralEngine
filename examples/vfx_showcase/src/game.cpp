#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/vfx_director/vfx_director.hpp"

#include <astral/app/feature.hpp>
#include <astral/gpu/device.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace astral;

namespace vfx_showcase {

namespace {

void draw_hud(Context& ctx, StationId station, const RenderSettings& s) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f;
    ctx.hud.text(font, info(station).title, {24 * unit, 18 * unit},
                 {.size = 38 * unit, .color = {1.0f, 0.85f, 0.45f, 1.0f}, .shadow = true});
    ctx.hud.text(font, info(station).hint, {24 * unit, 62 * unit},
                 {.size = 18 * unit, .color = {1.0f, 1.0f, 1.0f, 0.9f}, .shadow = true});

    const Atmosphere* atmo = ctx.singleton<Atmosphere>();
    char hour_buf[32] = "saat --";
    if (atmo) std::snprintf(hour_buf, sizeof(hour_buf), "saat %.1f", static_cast<double>(atmo->hour));
    const std::string mode = !s.ray_tracing ? "raster" : s.path_tracer ? "path tracer" : "hibrit RT";
    const std::string state = mode + "   " + hour_buf +
                              "   gölge:" + (s.fog_self_shadows ? "açık" : "kapalı") +
                              "   DDGI:" + (s.fog_ddgi ? "açık" : "kapalı") +
                              "   IBL:" + (s.sky_ibl ? "açık" : "kapalı");
    ctx.hud.text(font, state, {screen.x - 24 * unit, 18 * unit},
                 {.size = 16 * unit, .color = {0.75f, 0.9f, 1.0f, 0.92f}, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font,
                 "1/2/3/4 istasyon · Space VFX+ses · X durdur · C dumanı dağıt · T saat · F/G/I sis/IBL · WASD QE uç · sağ tık bak",
                 {screen.x * 0.5f, screen.y - 34 * unit},
                 {.size = 15 * unit, .color = {1.0f, 1.0f, 1.0f, 0.72f}, .align = TextAlign::Center});
}

void clear_station_vfx(Context& ctx) {
    ctx.world.each<const vfx_director::Beacon>([&](const vfx_director::Beacon& b) {
        if (!b.tag.empty()) ctx.world.send(StopVfx{.tag = b.tag});
    });
    std::vector<Entity> stray_sounds;
    ctx.world.each<const vfx_director::SoundOneShot>([&](Entity e, const vfx_director::SoundOneShot&) {
        stray_sounds.push_back(e);
    });
    for (const Entity e : stray_sounds) ctx.world.destroy(e);
}

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("vfx_director", vfx_director::install_vfx_director);
    return game.install(ctx);
}

ShowcaseGame::ShowcaseGame(Options options)
    : options_(options), station_(options.station), settings_(station_settings(options.station)) {
    settings_.ray_tracing = !options_.raster;
    settings_.path_tracer = options_.path_tracer && !options_.raster;
    settings_.fog_self_shadows = options_.fog_self_shadows;
    settings_.fog_ddgi = options_.fog_ddgi;
    settings_.sky_ibl = options_.sky_ibl;
}

void ShowcaseGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    if (ctx.device && !ctx.device->supports(gpu::Feature::RayTracing)) {
        options_.raster = true;
        settings_.ray_tracing = false;
        settings_.path_tracer = false;
    }
    show(ctx, station_);
    ctx.log.info("vfx showcase: 1/2/3/4 stations, Space VFX+sound, X stop VFX, C disperse smoke, T clock, F/G/I fog/IBL");
}

void ShowcaseGame::show(Context& ctx, StationId id) {
    clear_station_vfx(ctx);
    for (const Entity e : owned_) {
        if (ctx.world.alive(e)) ctx.world.destroy(e);
    }
    owned_.clear();
    station_ = id;

    StationBuild build{ctx, owned_, options_.overrides};
    switch (id) {
    case StationId::Courtyard: build_courtyard(build); break;
    case StationId::Particles: build_particles(build); break;
    case StationId::Smoke: build_smoke(build); break;
    case StationId::Atmosphere: build_atmosphere(build); break;
    }

    RenderSettings next = station_settings(id);
    next.ray_tracing = settings_.ray_tracing;
    next.path_tracer = settings_.path_tracer;
    next.fog_self_shadows = settings_.fog_self_shadows;
    next.fog_ddgi = settings_.fog_ddgi;
    next.sky_ibl = settings_.sky_ibl;
    settings_ = next;
    apply_settings(ctx);
    if (ctx.renderer) ctx.renderer->reset_history();
}

void ShowcaseGame::apply_settings(Context& ctx) {
    if (ctx.renderer) ctx.renderer->set_settings(settings_);
}

void ShowcaseGame::on_update(Context& ctx) {
    constexpr Key keys[] = {Key::Num1, Key::Num2, Key::Num3, Key::Num4};
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        const auto id = static_cast<StationId>(i);
        if (ctx.input.pressed(keys[i]) && id != station_) show(ctx, id);
    }
    bool changed = false;
    if (ctx.input.pressed(Key::F)) {
        settings_.fog_self_shadows = !settings_.fog_self_shadows;
        changed = true;
    }
    if (ctx.input.pressed(Key::G)) {
        settings_.fog_ddgi = !settings_.fog_ddgi;
        changed = true;
    }
    if (ctx.input.pressed(Key::I)) {
        settings_.sky_ibl = !settings_.sky_ibl;
        changed = true;
    }
    if (ctx.input.pressed(Key::T)) {
        if (auto* atmo = ctx.singleton_mut<Atmosphere>()) {
            atmo->hours_per_second = atmo->hours_per_second > 0.0f ? 0.0f : 0.5f;
        }
    }
    if (changed) apply_settings(ctx);
    draw_hud(ctx, station_, settings_);
}

void ShowcaseGame::on_shutdown(Context& ctx) {
    if (!options_.profile || !ctx.renderer) return;
    const auto stats = ctx.renderer->stats();
    ctx.log.info("vfx_showcase volume bricks: {}, budget fallbacks: {}", stats.volume_bricks, stats.volume_brick_fallbacks);
    for (const auto& timing : ctx.renderer->gpu_timings()) {
        ctx.log.info("vfx_showcase pass {}: {} ms", timing.name, timing.ms);
    }
}

} // namespace vfx_showcase
