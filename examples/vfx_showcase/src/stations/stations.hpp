#pragma once
// Four VFX stations, one loaded at a time: courtyard VFX timeline, particles + wind + volume flame,
// gameplay smoke + dispersion, and atmosphere + aerial perspective + froxel fog.

#include <astral/astral.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace vfx_showcase {

enum class StationId : std::uint8_t { Courtyard, Particles, Smoke, Atmosphere };

struct StationInfo {
    const char* name;  ///< Command line: --view <name>.
    const char* title; ///< HUD heading.
    const char* hint;  ///< HUD description: what to look at.
};

struct StationOverrides {
    std::optional<float> hour;
    std::optional<float> hours_per_second;
    std::optional<float> fog;
    bool baseline = false;
    bool aerial = true;
};

[[nodiscard]] const StationInfo& info(StationId id);
/// Unknown names resolve to Courtyard.
[[nodiscard]] StationId station_from_name(std::string_view name);
[[nodiscard]] astral::RenderSettings station_settings(StationId id);

struct StationBuild {
    astral::Context& ctx;
    std::vector<astral::Entity>& owned;
    const StationOverrides& overrides;

    template <class... Cs> astral::Entity create(Cs&&... components) {
        const astral::Entity e = ctx.world.create(std::forward<Cs>(components)...);
        owned.push_back(e);
        return e;
    }

    bool load_courtyard(float default_hour, float default_fog, bool add_colliders);
    astral::Entity camera(astral::Vec3 eye, astral::Vec3 target, float fov_degrees);
};

void build_courtyard(StationBuild& build);
void build_particles(StationBuild& build);
void build_smoke(StationBuild& build);
void build_atmosphere(StationBuild& build);

} // namespace vfx_showcase
