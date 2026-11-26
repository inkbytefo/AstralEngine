#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/orbits/orbits.hpp"

#include <astral/app/feature.hpp>
#include <astral/gpu/device.hpp>

#include <string>

using namespace astral;

namespace lighting_showcase {

namespace {

constexpr const char* debug_view_names[] = {"kapalı", "albedo", "normal", "yaygın ışık", "speküler", "hareket"};
constexpr std::uint32_t debug_view_count = 6;
static_assert(static_cast<std::uint32_t>(DebugView::Motion) + 1 == debug_view_count);

void draw_hud(Context& ctx, StationId station, const RenderSettings& s) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f;
    ctx.hud.text(font, info(station).title, {24 * unit, 18 * unit}, {.size = 40 * unit, .color = {1, 0.85f, 0.45f, 1}, .shadow = true});
    ctx.hud.text(font, info(station).hint, {24 * unit, 64 * unit}, {.size = 18 * unit, .color = {1, 1, 1, 0.9f}, .shadow = true});
    const std::string mode = !s.ray_tracing ? "raster" : s.path_tracer ? "path tracer" : "hibrit ışın izleme";
    const std::string state = mode + "   ReSTIR " + (s.restir_di ? "açık" : "kapalı") + "   debug: " +
                              debug_view_names[static_cast<std::uint32_t>(s.debug_view)];
    ctx.hud.text(font, state, {screen.x - 24 * unit, 18 * unit},
                 {.size = 16 * unit, .color = {0.75f, 0.9f, 1, 0.9f}, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font, "1/2/3 istasyon · P path tracer · R ReSTIR · V debug görünümü · WASD QE uç · sağ tık bak · Esc çıkış",
                 {screen.x * 0.5f, screen.y - 34 * unit}, {.size = 16 * unit, .color = {1, 1, 1, 0.7f}, .align = TextAlign::Center});
}

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("orbits", orbits::install_orbits);
    return game.install(ctx);
}

ShowcaseGame::ShowcaseGame(Options options) : options_(options), station_(options.station), settings_(station_settings(options.station)) {
    settings_.ray_tracing = !options_.raster;
    settings_.path_tracer = options_.path_tracer && !options_.raster;
    settings_.debug_view = options_.debug_view;
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
    ctx.log.info("lighting showcase: 1/2/3 stations, P path tracer, R ReSTIR, V debug view, WASD+QE fly, right mouse look");
}

void ShowcaseGame::show(Context& ctx, StationId id) {
    for (const Entity e : owned_) ctx.world.destroy(e);
    owned_.clear();
    station_ = id;
    resources_.rewind();
    StationBuild build{ctx, resources_, owned_, options_.raster};
    switch (id) {
    case StationId::Room: build_room(build); break;
    case StationId::Yard: build_yard(build); break;
    case StationId::Night: build_night(build); break;
    }
    // The station's own GI volume and bloom; the player's switches stay.
    RenderSettings next = station_settings(id);
    next.ray_tracing = settings_.ray_tracing;
    next.path_tracer = settings_.path_tracer;
    next.restir_di = settings_.restir_di;
    next.debug_view = settings_.debug_view;
    settings_ = next;
    apply_settings(ctx);
    if (ctx.renderer) ctx.renderer->reset_history();
}

void ShowcaseGame::apply_settings(Context& ctx) {
    // The path tracer accumulates only while nothing moves.
    const bool frozen = settings_.path_tracer;
    ctx.world.each<orbits::Orbit>([&](orbits::Orbit& o) { o.frozen = frozen; });
    ctx.world.each<orbits::Tumble>([&](orbits::Tumble& t) { t.frozen = frozen; });
    if (ctx.renderer) ctx.renderer->set_settings(settings_);
}

void ShowcaseGame::on_update(Context& ctx) {
    constexpr Key keys[] = {Key::Num1, Key::Num2, Key::Num3};
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        const auto id = static_cast<StationId>(i);
        if (ctx.input.pressed(keys[i]) && id != station_) show(ctx, id);
    }
    bool changed = false;
    if (ctx.input.pressed(Key::P) && !options_.raster) {
        settings_.path_tracer = !settings_.path_tracer;
        changed = true;
    }
    if (ctx.input.pressed(Key::R)) {
        settings_.restir_di = !settings_.restir_di;
        changed = true;
    }
    if (ctx.input.pressed(Key::V)) {
        settings_.debug_view = static_cast<DebugView>((static_cast<std::uint32_t>(settings_.debug_view) + 1) % debug_view_count);
        changed = true;
    }
    if (changed) apply_settings(ctx);
    draw_hud(ctx, station_, settings_);
}

} // namespace lighting_showcase
