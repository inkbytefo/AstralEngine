#include "features/day_night/day_night.hpp"

#include <cstdio>

using namespace astral;

namespace day_night {

namespace {

/// N flips the state; a changed state is applied once.
class DayNightSystem final : public System {
public:
    void update(Context& ctx) override {
        const bool toggle = ctx.actions.pressed("toggle_night");
        ctx.world.each<DayNight>([&](DayNight& d) {
            if (toggle) d.night = !d.night;
            if (d.applied != (d.night ? 1 : 0)) {
                apply(ctx, d.night);
                d.applied = d.night ? 1 : 0;
            }
        });
    }
};

class StreamingHud final : public System {
public:
    void update(Context& ctx) override {
        const StreamingStats* s = ctx.maps().world_streaming();
        if (!s) return;
        bool night = false;
        ctx.world.each<const DayNight>([&](const DayNight& d) { night = d.night; });
        const WorldPosition w = ctx.maps().world_origin();
        const Vec2 screen = ctx.screen_size();
        const float unit = screen.y / 720.0f;
        char line[160];
        std::snprintf(line, sizeof line, "cells %u  loading %u  |  origin %.0f, %.0f  |  %s", s->attached, s->loading + s->waiting, w.x, w.z,
                      night ? "night (N)" : "day (N)");
        ctx.hud.text(*ctx.font, line, {16 * unit, 16 * unit}, {.size = 18 * unit, .color = {1, 1, 1, 0.85f}, .shadow = true});
    }
};

} // namespace

void apply(Context& ctx, bool night) {
    if (MapLayers* layers = ctx.maps().world_layers()) layers->set("night", night);
    ctx.sun.intensity = night ? 0.08f : 3.0f;
    ctx.sun.color = night ? Vec3{0.6f, 0.7f, 1.0f} : Vec3{1, 1, 1};
    ctx.environment.sky = night ? Vec3{0.02f, 0.03f, 0.06f} : Vec3{0.45f, 0.6f, 0.85f};
    ctx.environment.ambient = night ? 0.15f : 0.35f;
    ctx.environment.exposure = night ? 2.5f : 1.0f;
}

void install_day_night(FeatureContext& feature) {
    feature.component<DayNight>();
    feature.system<DayNightSystem>(Phase::Update);
    feature.system<StreamingHud>(Phase::PostUpdate);
}

} // namespace day_night
