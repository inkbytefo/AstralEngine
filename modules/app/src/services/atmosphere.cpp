#include "atmosphere.hpp"

#include "render_bridge.hpp"

#include <astral/app/app.hpp>

#include <algorithm>
#include <cmath>

namespace astral {
namespace {
float finite_or(float value, float fallback = 0.0f) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float wrap_hour(double hour) noexcept {
    const double wrapped = std::fmod(hour, 24.0);
    return static_cast<float>(wrapped < 0 ? wrapped + 24.0 : wrapped);
}
} // namespace

AtmosphereLighting evaluate_atmosphere(const Atmosphere& settings) noexcept {
    const float angle = radians((wrap_hour(finite_or(settings.hour, 12)) - 12.0f) * 15.0f);
    const float tilt = radians(std::clamp(finite_or(settings.orbit_tilt_deg), -89.0f, 89.0f));
    const float heading = radians(std::remainder(finite_or(settings.heading_deg), 360.0f));
    const float height = std::cos(angle);
    const Vec3 orbit{std::sin(angle), std::cos(tilt) * height, std::sin(tilt) * height};
    const Vec3 toward{std::cos(heading) * orbit.x + std::sin(heading) * orbit.z, orbit.y,
                      -std::sin(heading) * orbit.x + std::cos(heading) * orbit.z};

    // Effective air mass for a finite spherical atmosphere at ground level.
    const float mu = std::max(toward.y, 0.0f);
    const float air_mass = 1.0f / (mu + 0.025f * std::exp(-11.0f * mu));
    const Vec3 transmission{std::exp(-0.065f * air_mass), std::exp(-0.115f * air_mass),
                            std::exp(-0.23f * air_mass)};
    const float direct = std::clamp(toward.y * 20.0f, 0.0f, 1.0f);
    const float daylight = std::clamp((toward.y + 0.15f) / 0.4f, 0.0f, 1.0f);
    const Vec3 night{0.008f, 0.014f, 0.032f};
    const Vec3 day{0.35f, 0.52f, 0.86f};
    const Vec3 sky = night * (1.0f - daylight) + day * daylight;
    return {toward, transmission, sky, std::max(finite_or(settings.sun_intensity), 0.0f) * direct};
}

void AtmosphereController::authored_lighting(const Context& ctx, DirectionalLight& sun, Environment& environment) const {
    sun = ctx.sun;
    environment = ctx.environment;
    if (authored_) {
        sun.direction = authored_->sun.direction;
        sun.color = authored_->sun.color;
        sun.intensity = authored_->sun.intensity;
        environment.sky = authored_->sky_light;
    }
}

void AtmosphereController::set_authored_lighting(Context& ctx, const DirectionalLight& sun, const Environment& environment) {
    if (authored_) {
        authored_->sun = sun;
        authored_->sky_light = environment.sky;
    }
    ctx.sun = sun;
    ctx.environment = environment;
}

void AtmosphereController::update(Context& ctx, RenderBridge& render, float delta) {
    Entity selected;
    ctx.world.each<const Atmosphere>([&](Entity entity, const Atmosphere& settings) {
        if (settings.enabled && (!selected.valid() || entity.index < selected.index)) selected = entity;
    });
    if (!selected.valid()) {
        if (!authored_) return;
        ctx.sun.direction = authored_->sun.direction;
        ctx.sun.color = authored_->sun.color;
        ctx.sun.intensity = authored_->sun.intensity;
        ctx.environment.sky = authored_->sky_light;
        if (overrides_sky_) ctx.sky = authored_->sky_material;
        authored_.reset();
        overrides_sky_ = false;
        return;
    }

    if (!authored_) authored_ = Authored{ctx.sun, ctx.environment.sky, ctx.sky};
    const auto* settings = ctx.world.get<const Atmosphere>(selected);
    const double elapsed = static_cast<double>(finite_or(delta)) * finite_or(settings->hours_per_second);
    if (elapsed != 0.0) {
        ctx.world.get<Atmosphere>(selected)->hour = wrap_hour(static_cast<double>(finite_or(settings->hour, 12)) + elapsed);
    }
    const auto lighting = evaluate_atmosphere(*settings);
    ctx.sun.direction = lighting.toward_sun * -1.0f;
    ctx.sun.color = lighting.transmission;
    ctx.sun.intensity = lighting.sun_intensity;
    ctx.environment.sky = lighting.sky;
    if (ctx.renderer && !settings->sky_material.empty()) {
        const auto material = render.load_material(settings->sky_material);
        if (material.valid()) {
            ctx.sky = material;
            overrides_sky_ = true;
            return;
        }
    }
    if (overrides_sky_) ctx.sky = authored_->sky_material;
    overrides_sky_ = false;
}

} // namespace astral
