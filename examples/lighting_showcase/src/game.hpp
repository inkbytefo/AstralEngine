#pragma once
// lighting_showcase: three lighting stations, one loaded at a time. Keys: 1/2/3 stations, P path tracer,
// R ReSTIR, V debug view.

#include "stations/stations.hpp"

#include <astral/astral.hpp>

#include <vector>

namespace lighting_showcase {

bool install_features(astral::Context& ctx);

struct Options {
    StationId station = StationId::Room;
    bool raster = false;
    bool path_tracer = false;
    astral::DebugView debug_view = astral::DebugView::None;
};

class ShowcaseGame final : public astral::Game {
public:
    explicit ShowcaseGame(Options options = {});
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

    [[nodiscard]] StationId station() const noexcept { return station_; }
    /// The renderer settings the game wants now (also without a renderer).
    [[nodiscard]] const astral::RenderSettings& settings() const noexcept { return settings_; }

private:
    void show(astral::Context& ctx, StationId id);
    void apply_settings(astral::Context& ctx);

    Options options_;
    StationId station_;
    astral::RenderSettings settings_;
    StationResources resources_;
    std::vector<astral::Entity> owned_;
};

} // namespace lighting_showcase
