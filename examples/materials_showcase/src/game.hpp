#pragma once
// materials_showcase: three stations, one loaded at a time (studio, workflow, gallery).
// Keys: 1/2/3 stations, P path tracer, U UV/texel density inspection (workflow), WASD+QE fly, right mouse look.

#include "stations/stations.hpp"

#include <astral/astral.hpp>

#include <vector>

namespace materials_showcase {

bool install_features(astral::Context& ctx);

struct Options {
    StationId station = StationId::Studio;
    bool raster = false;
    bool path_tracer = false;
    UvInspectMode uv_mode = UvInspectMode::Off;
};

class ShowcaseGame final : public astral::Game {
public:
    explicit ShowcaseGame(Options options = {});
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

    [[nodiscard]] StationId station() const noexcept { return station_; }
    [[nodiscard]] UvInspectMode uv_mode() const noexcept { return uv_mode_; }
    [[nodiscard]] const astral::RenderSettings& settings() const noexcept { return settings_; }

private:
    void show(astral::Context& ctx, StationId id);
    void apply_settings(astral::Context& ctx);

    Options options_;
    StationId station_;
    UvInspectMode uv_mode_ = UvInspectMode::Off;
    astral::RenderSettings settings_;
    StationResources resources_;
    WorkflowState workflow_;
    std::vector<astral::Entity> owned_;
};

} // namespace materials_showcase
