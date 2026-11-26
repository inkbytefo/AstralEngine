#pragma once
// vfx_showcase: four stations demonstrating particles, procedural volumes, gameplay smoke,
// atmosphere, froxel fog, sky IBL, and Luau-driven VFX and sound.

#include "stations/stations.hpp"

#include <astral/astral.hpp>

#include <vector>

namespace vfx_showcase {

bool install_features(astral::Context& ctx);

struct Options {
    StationId station = StationId::Courtyard;
    StationOverrides overrides{};
    bool raster = false;
    bool path_tracer = false;
    bool fog_self_shadows = true;
    bool fog_ddgi = true;
    bool sky_ibl = true;
    bool profile = false;
};

class ShowcaseGame final : public astral::Game {
public:
    explicit ShowcaseGame(Options options = {});
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;
    void on_shutdown(astral::Context& ctx) override;

    [[nodiscard]] StationId station() const noexcept { return station_; }
    [[nodiscard]] const astral::RenderSettings& settings() const noexcept { return settings_; }

private:
    void show(astral::Context& ctx, StationId id);
    void apply_settings(astral::Context& ctx);

    Options options_;
    StationId station_;
    astral::RenderSettings settings_;
    std::vector<astral::Entity> owned_;
};

} // namespace vfx_showcase
