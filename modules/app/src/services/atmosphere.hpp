#pragma once

#include <astral/app/components.hpp>

#include <optional>

namespace astral {
class Context;
class RenderBridge;

struct AtmosphereLighting {
    Vec3 toward_sun;
    Vec3 transmission;
    Vec3 sky;
    float sun_intensity;
};

AtmosphereLighting evaluate_atmosphere(const Atmosphere& settings) noexcept;

class AtmosphereController {
public:
    void update(Context& context, RenderBridge& render, float delta);
    void authored_lighting(const Context& ctx, DirectionalLight& sun, Environment& environment) const;
    void set_authored_lighting(Context& ctx, const DirectionalLight& sun, const Environment& environment);

private:
    struct Authored {
        DirectionalLight sun;
        Vec3 sky_light;
        MaterialHandle sky_material;
    };
    std::optional<Authored> authored_;
    bool overrides_sky_ = false;
};

} // namespace astral
