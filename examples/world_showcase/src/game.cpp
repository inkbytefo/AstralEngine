#include "game.hpp"

#include "features/day_night/day_night.hpp"
#include "features/fly_camera/fly_camera.hpp"

#include <astral/app/feature.hpp>
#include <astral/gpu/device.hpp>

#include <cstdio>
#include <iterator>
#include <string>

using namespace astral;

namespace world_showcase {

namespace {

std::string stats_line(Context& ctx, StationId station, bool far_view, std::uint32_t shifts, bool raster) {
    const day_night::DayNight* dn = ctx.singleton<day_night::DayNight>();
    const bool night = dn && dn->night;
    const char* mode = raster ? "raster" : "ışın izleme";
    char buf[220];
    if (const StreamingStats* s = ctx.maps().world_streaming()) {
        const WorldPosition o = ctx.maps().world_origin();
        std::snprintf(buf, sizeof buf, "hücre %u (+%u)  |  orijin %.0f, %.0f (kayma %u)  |  %s  |  %s",
                      s->attached, s->loading + s->waiting, o.x, o.z, shifts, night ? "gece (N)" : "gündüz (N)", mode);
        return buf;
    }
    const std::size_t bodies = ctx.physics ? ctx.physics->body_count() : 0;
    const char* lod = (station == StationId::Hill) ? (far_view ? "uzak açı (F)" : "karışık LOD (F)") : "akış aktif";
    std::snprintf(buf, sizeof buf, "arazi karosu %zu  |  %s  |  %s  |  %s", bodies, lod,
                  night ? "gece (N)" : "gündüz (N)", mode);
    return buf;
}

void draw_hud(Context& ctx, StationId station, bool far_view, std::uint32_t shifts, bool raster) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f;
    ctx.hud.text(font, info(station).title, {24 * unit, 18 * unit},
                 {.size = 38 * unit, .color = {1, 0.85f, 0.45f, 1}, .shadow = true});
    ctx.hud.text(font, info(station).hint, {24 * unit, 62 * unit},
                 {.size = 18 * unit, .color = {1, 1, 1, 0.9f}, .shadow = true});
    ctx.hud.text(font, stats_line(ctx, station, far_view, shifts, raster), {screen.x - 24 * unit, 18 * unit},
                 {.size = 16 * unit, .color = {0.75f, 0.9f, 1, 0.9f}, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font, "1/2/3 istasyon · N gündüz/gece · C hücre çizgileri · F tepe uzak açı · WASD QE uç · sağ tık bak · Esc çıkış",
                 {screen.x * 0.5f, screen.y - 34 * unit},
                 {.size = 16 * unit, .color = {1, 1, 1, 0.7f}, .align = TextAlign::Center});
}

void reset_hill_camera(Context& ctx, bool far_view) {
    ctx.world.each<Transform, fly::FlyCamera>([&](Transform& t, fly::FlyCamera& cam) {
        t.position = hill_camera_eye(far_view);
        cam.yaw = 0.0f;
        cam.pitch = radians(-43.0f);
        t.rotation = Quat::from_axis_angle({1, 0, 0}, cam.pitch);
    });
}

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("day_night", day_night::install_day_night);
    return game.install(ctx);
}

ShowcaseGame::ShowcaseGame(Options options)
    : options_(options), station_(options.station), far_view_(options.far_view) {
    render_settings_.ray_tracing = !options_.raster;
}

void ShowcaseGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    if (ctx.device && !ctx.device->supports(gpu::Feature::RayTracing)) {
        options_.raster = true;
        render_settings_.ray_tracing = false;
        if (ctx.renderer) ctx.renderer->set_settings(render_settings_);
    }
    ctx.world.create(day_night::DayNight{.night = options_.night, .applied = -1});
    ctx.debug_show.world_cells = options_.show_cells;
    if (options_.capturing) {
        ctx.add_system(Phase::PostUpdate, "world_showcase::FinishLoading",
                       [](Context& c) { c.maps().finish_world_loading(); });
    }
    show(ctx, station_);
    ctx.log.info("world showcase: 1/2/3 stations, N day/night, C cells overlay, F hill far view, WASD+QE fly, right mouse look");
}

void ShowcaseGame::show(Context& ctx, StationId id) {
    ctx.maps().close_world();
    for (const Entity e : owned_) {
        if (ctx.world.alive(e)) ctx.world.destroy(e);
    }
    owned_.clear();
    station_ = id;

    StationBuild build{ctx, owned_, options_.capturing, far_view_, options_.nudge};
    bool ok = false;
    switch (id) {
    case StationId::World: ok = build_world_station(build); break;
    case StationId::Range: ok = build_range_station(build); break;
    case StationId::Hill: ok = build_hill_station(build); break;
    }
    if (!ok) {
        ctx.log.error("world_showcase: failed to load station {}", info(id).name);
        ctx.quit();
        return;
    }

    if (day_night::DayNight* dn = ctx.singleton_mut<day_night::DayNight>()) {
        day_night::apply(ctx, dn->night);
        dn->applied = dn->night ? 1 : 0;
    }
    ctx.maps().finish_world_loading();
    if (ctx.renderer) ctx.renderer->reset_history();
}

void ShowcaseGame::on_update(Context& ctx) {
    origin_events_.read(ctx.world, [&](const OriginShifted&) { ++origin_shifts_; });

    constexpr Key keys[] = {Key::Num1, Key::Num2, Key::Num3};
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        const auto id = static_cast<StationId>(i);
        if (ctx.input.pressed(keys[i]) && id != station_) show(ctx, id);
    }
    if (ctx.input.pressed(Key::C)) ctx.debug_show.world_cells = !ctx.debug_show.world_cells;
    if (ctx.input.pressed(Key::F) && station_ == StationId::Hill) {
        far_view_ = !far_view_;
        reset_hill_camera(ctx, far_view_);
    }
    draw_hud(ctx, station_, far_view_, origin_shifts_, options_.raster);
}

} // namespace world_showcase
