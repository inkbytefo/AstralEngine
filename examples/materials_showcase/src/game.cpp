#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <astral/app/feature.hpp>
#include <astral/gpu/device.hpp>

#include <string>

using namespace astral;

namespace materials_showcase {

namespace {

constexpr const char* uv_mode_names[] = {"kapalı", "UV dama", "texel yoğunluğu"};

void destroy_station_entities(World& world, std::vector<Entity>& owned) {
    for (const Entity e : owned) {
        if (world.alive(e)) world.destroy(e);
    }
    owned.clear();
    // Immediately destroy spawned descendants (such as ModelRef piece children) so no stale meshes survive a frame.
    bool removed = true;
    while (removed) {
        std::vector<Entity> orphans;
        world.each<const Parent>([&](Entity e, const Parent& p) {
            if (!world.alive(p.entity)) orphans.push_back(e);
        });
        removed = !orphans.empty();
        for (const Entity e : orphans) {
            if (world.alive(e)) world.destroy(e);
        }
    }
}

void draw_hud(Context& ctx, StationId station, const RenderSettings& s, UvInspectMode uv_mode) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float unit = screen.y / 720.0f;
    ctx.hud.text(font, info(station).title, {24 * unit, 18 * unit},
                 {.size = 40 * unit, .color = {1, 0.85f, 0.45f, 1}, .shadow = true});
    ctx.hud.text(font, info(station).hint, {24 * unit, 64 * unit},
                 {.size = 18 * unit, .color = {1, 1, 1, 0.9f}, .shadow = true});
    std::string state = !s.ray_tracing ? "raster" : s.path_tracer ? "path tracer" : "hibrit ışın izleme";
    if (station == StationId::Workflow) {
        state += "   UV: ";
        state += uv_mode_names[static_cast<std::size_t>(uv_mode)];
    }
    ctx.hud.text(font, state, {screen.x - 24 * unit, 18 * unit},
                 {.size = 16 * unit, .color = {0.75f, 0.9f, 1, 0.9f}, .align = TextAlign::Right, .shadow = true});
    ctx.hud.text(font, "1/2/3 istasyon · P path tracer · U UV/yoğunluk · WASD QE uç · sağ tık bak · F1 debug · Esc çıkış",
                 {screen.x * 0.5f, screen.y - 34 * unit},
                 {.size = 16 * unit, .color = {1, 1, 1, 0.85f}, .align = TextAlign::Center, .shadow = true});
}

} // namespace

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    return game.install(ctx);
}

ShowcaseGame::ShowcaseGame(Options options)
    : options_(options), station_(options.station), settings_(station_settings(options.station)) {
    settings_.ray_tracing = !options_.raster;
    settings_.path_tracer = options_.path_tracer && !options_.raster;
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
    if (station_ == StationId::Workflow && options_.uv_mode != UvInspectMode::Off) {
        uv_mode_ = options_.uv_mode;
        apply_workflow_uv_mode(ctx, workflow_, uv_mode_);
    }
    ctx.log.info("materials showcase: 1/2/3 stations, P path tracer, U UV/density (workflow), WASD+QE fly, right mouse look");
}

void ShowcaseGame::show(Context& ctx, StationId id) {
    destroy_station_entities(ctx.world, owned_);
    workflow_.clear();
    uv_mode_ = UvInspectMode::Off;
    ctx.sky = {};
    ctx.post.clear();
    ctx.sun = {};
    ctx.environment = {};
    station_ = id;
    resources_.rewind();

    StationBuild build{ctx, resources_, owned_, workflow_, options_.raster};
    switch (id) {
    case StationId::Studio: build_studio(build); break;
    case StationId::Workflow: build_workflow(build); break;
    case StationId::Gallery: build_gallery(build); break;
    }

    RenderSettings next = station_settings(id);
    next.ray_tracing = settings_.ray_tracing;
    next.path_tracer = settings_.path_tracer;
    settings_ = next;
    apply_settings(ctx);
    if (ctx.renderer) ctx.renderer->reset_history();
}

void ShowcaseGame::apply_settings(Context& ctx) {
    if (ctx.renderer) ctx.renderer->set_settings(settings_);
}

void ShowcaseGame::on_update(Context& ctx) {
    constexpr Key keys[] = {Key::Num1, Key::Num2, Key::Num3};
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        const auto id = static_cast<StationId>(i);
        if (ctx.input.pressed(keys[i]) && id != station_) show(ctx, id);
    }
    if (ctx.input.pressed(Key::P) && !options_.raster) {
        settings_.path_tracer = !settings_.path_tracer;
        apply_settings(ctx);
    }
    if (ctx.input.pressed(Key::U) && station_ == StationId::Workflow) {
        uv_mode_ = static_cast<UvInspectMode>((static_cast<std::uint8_t>(uv_mode_) + 1u) % 3u);
        apply_workflow_uv_mode(ctx, workflow_, uv_mode_);
    }
    draw_hud(ctx, station_, settings_, uv_mode_);
}

} // namespace materials_showcase
