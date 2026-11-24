#pragma once

#include <astral/core/math.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace astral {

class PhysicsService;

struct GameplaySmokeDesc {
    unsigned radius_cells = 8;          ///< Grid half-width, clamped to 1..12.
    float cell_size_m = 0.25f;
    float extinction_per_m = 2.0f;     ///< At full density.
    unsigned spread_interval_steps = 2;
    unsigned spread_steps = 240;       ///< Spread and hold for this many fixed ticks.
    unsigned fade_steps = 90;           ///< Linear decay after spreading stops.
    std::uint32_t obstacle_layers = ~0u;
};

/// Runtime component for a root entity with Transform. App advances it once per fixed tick.
/// Density samples are binary occupancy in X-major, then Y, then Z order. density_scale()
/// gives their common fade multiplier. The Transform locates the centre cell.
class GameplaySmoke {
public:
    explicit GameplaySmoke(GameplaySmokeDesc desc = {});

    void step(const PhysicsService& physics, Vec3 centre);
    [[nodiscard]] float optical_depth(Vec3 centre, Vec3 from, Vec3 to) const noexcept;
    [[nodiscard]] float transmittance(Vec3 centre, Vec3 from, Vec3 to) const noexcept;
    [[nodiscard]] float density_scale() const noexcept;
    [[nodiscard]] float extinction_per_m() const noexcept { return desc_.extinction_per_m * density_scale(); }
    [[nodiscard]] unsigned side() const noexcept { return side_; }
    [[nodiscard]] float cell_size_m() const noexcept { return desc_.cell_size_m; }
    [[nodiscard]] std::span<const std::uint8_t> occupancy() const noexcept { return occupied_; }

private:
    GameplaySmokeDesc desc_;
    unsigned side_;
    std::vector<std::uint8_t> occupied_;
    std::uint64_t tick_ = 0;
};

} // namespace astral
