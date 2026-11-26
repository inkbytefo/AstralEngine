#pragma once
// world_showcase: streamed 4 x 4 km map with floating origin and night layer, 6 x 6 streaming mountain range terrain
// with height/slope layers, and 2 x 2 LOD hill terrain. Keys: 1/2/3 stations, N day/night, C world cells, F hill far.

#include "stations/stations.hpp"

#include <astral/astral.hpp>

#include <cstdint>
#include <vector>

namespace world_showcase {

bool install_features(astral::Context& ctx);

struct Options {
    StationId station = StationId::World;
    bool capturing = false;
    bool night = false;
    bool far_view = false;
    bool raster = false;
    bool show_cells = false;
    float nudge = 0.0f; ///< Metres added to world station start Z (stability test).
};

class ShowcaseGame final : public astral::Game {
public:
    explicit ShowcaseGame(Options options = {});
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

    [[nodiscard]] StationId station() const noexcept { return station_; }
    [[nodiscard]] bool far_view() const noexcept { return far_view_; }
    [[nodiscard]] std::uint32_t origin_shifts() const noexcept { return origin_shifts_; }
    [[nodiscard]] const astral::RenderSettings& render_settings() const noexcept { return render_settings_; }

private:
    void show(astral::Context& ctx, StationId id);

    Options options_;
    StationId station_;
    bool far_view_ = false;
    std::uint32_t origin_shifts_ = 0;
    astral::RenderSettings render_settings_{};
    astral::EventReader<astral::OriginShifted> origin_events_;
    std::vector<astral::Entity> owned_;
};

} // namespace world_showcase
