#pragma once
// Fields (ADR-0029): wind and water as pure functions of position and time. The same functions run in shaders
// (shaders/include/astral/fields.glsl: same hash, same noise, same formulas), so simulation (CPU) and
// rendering, vegetation, particles and clouds (GPU) read one wind. Nothing here is stateful: no update step,
// no randomness except the seeded hash.

#include <astral/core/math.hpp>

#include <cstdint>
#include <vector>

namespace astral {

/// 32-bit integer hash (lowbias32) and value noise on it: identical in fields.glsl.
[[nodiscard]] std::uint32_t field_hash(std::uint32_t x) noexcept;
/// Smooth value noise in [0, 1): trilinear between hashed lattice values, smoothstep-eased.
[[nodiscard]] float field_noise(Vec3 p, std::uint32_t seed) noexcept;

/// One source of wind: a steady velocity with gusts along it and turbulence around it.
struct WindDesc {
    Vec3 velocity{};               ///< m/s.
    float gust_strength = 0.0f;    ///< Gusts vary the speed along the wind by +-this (m/s).
    float gust_frequency = 0.2f;   ///< Gusts per second.
    float turbulence = 0.0f;       ///< Random swirl: each axis varies by +-this (m/s).
    float turbulence_scale = 8.0f; ///< Size of the swirls (m).
    std::uint32_t seed = 1;
};

/// A box of wind (axis-aligned). Inside it adds `wind` to the world's, or replaces it (`replace`); `falloff` fades it out over that
/// many metres at the edge.
struct WindVolume {
    Vec3 center{};
    Vec3 half_extents{1, 1, 1};
    WindDesc wind{};
    float falloff = 0.0f;
    bool replace = false;
};

/// A box of water: its top is the surface (center.y + half_extents.y), waves ride on it.
struct WaterVolume {
    Vec3 center{};
    Vec3 half_extents{1, 1, 1};
    float density = 1000.0f; ///< kg/m^3 (sea water 1025).
    Vec3 flow{};             ///< Current, m/s.
    float wave_height = 0.0f; ///< Waves rise and fall by +-this (m).
    float wave_length = 8.0f;
    float wave_speed = 1.0f;  ///< m/s.
};

struct WaterSample {
    bool inside = false;      ///< The point is under the surface of a volume.
    float surface_height = 0.0f; ///< World y of the water surface there (waves included).
    float depth = 0.0f;       ///< Metres under the surface (0 when not inside).
    Vec3 flow{};
    float density = 0.0f;
};

/// Wind of one source at a point and time (also what the shader computes).
[[nodiscard]] Vec3 wind_of(const WindDesc& wind, Vec3 position, float time) noexcept;
/// 0 outside the box, 1 deeper than `falloff` inside it, eased in between.
[[nodiscard]] float volume_weight(Vec3 position, Vec3 center, Vec3 half_extents, float falloff) noexcept;
/// Water surface height of a volume at x, z and time.
[[nodiscard]] float water_surface_height(const WaterVolume& water, float x, float z, float time) noexcept;

/// Air density (kg/m^3) for drag and lift.
constexpr float air_density = 1.225f;

class FieldWorld {
public:
    void set_global_wind(const WindDesc& wind) { global_ = wind; }
    void add_wind_volume(const WindVolume& volume) { winds_.push_back(volume); }
    void add_water_volume(const WaterVolume& volume) { waters_.push_back(volume); }
    /// Removes every volume (the global wind stays). The App rebuilds the volumes from entities every step.
    void clear_volumes() {
        winds_.clear();
        waters_.clear();
    }

    [[nodiscard]] Vec3 wind_at(Vec3 position, double time) const noexcept;
    [[nodiscard]] WaterSample water_at(Vec3 position, double time) const noexcept;

    [[nodiscard]] const WindDesc& global_wind() const noexcept { return global_; }
    [[nodiscard]] const std::vector<WindVolume>& wind_volumes() const noexcept { return winds_; }
    [[nodiscard]] const std::vector<WaterVolume>& water_volumes() const noexcept { return waters_; }

private:
    WindDesc global_{};
    std::vector<WindVolume> winds_;
    std::vector<WaterVolume> waters_;
};

} // namespace astral
