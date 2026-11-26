#include "features/day_night/day_night.hpp"

using namespace astral;

namespace day_night {

void apply(Context& ctx, bool night) {
    if (MapLayers* layers = ctx.maps().world_layers()) layers->set("night", night);
    ctx.sun.intensity = night ? 0.08f : 3.0f;
    ctx.sun.color = night ? Vec3{0.6f, 0.7f, 1.0f} : Vec3{1.0f, 1.0f, 1.0f};
    ctx.environment.sky = night ? Vec3{0.02f, 0.03f, 0.06f} : Vec3{0.45f, 0.6f, 0.85f};
    ctx.environment.ambient = night ? 0.15f : 0.35f;
    ctx.environment.exposure = night ? 2.2f : 0.6f;
}

void install_day_night(FeatureContext& feature) {
    feature.component<DayNight>();
    feature.system(Phase::Update, "day_night::DayNightSystem", [](Context& ctx) {
        const bool toggle = ctx.actions.pressed("toggle_night");
        ctx.world.each<DayNight>([&](DayNight& d) {
            if (toggle) d.night = !d.night;
            if (d.applied != (d.night ? 1 : 0)) {
                apply(ctx, d.night);
                d.applied = d.night ? 1 : 0;
            }
        });
    });
}

} // namespace day_night
