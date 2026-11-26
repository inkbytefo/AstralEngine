#pragma once
// The three stations of world_showcase. One station is loaded at a time so the open map, streaming terrain,
// and LOD hill each own the scene, lighting and streaming focus cleanly.

#include <astral/astral.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace world_showcase {

enum class StationId : std::uint8_t { World, Range, Hill };

struct StationInfo {
    const char* name;  ///< Command line: --view <name>.
    const char* title; ///< HUD title.
    const char* hint;  ///< HUD one-line description of what to look at.
};

[[nodiscard]] const StationInfo& info(StationId id);
/// Unknown names default to World.
[[nodiscard]] StationId station_from_name(std::string_view name);
/// Resolves `--view` and `--far` together: if `--view` is omitted and `--far` is set, starts on Hill.
[[nodiscard]] StationId resolve_initial_station(std::string_view view_name, bool far_view);

/// What a station builder receives. Every entity it creates is tracked in `owned` so switching stations
/// destroys them cleanly without touching persistent singletons (such as DayNight).
struct StationBuild {
    astral::Context& ctx;
    std::vector<astral::Entity>& owned;
    bool capturing = false;
    bool far_view = false;
    float nudge = 0.0f;

    template <class... Cs> astral::Entity create(Cs&&... components) {
        const astral::Entity e = ctx.world.create(std::forward<Cs>(components)...);
        owned.push_back(e);
        return e;
    }

    /// Appends a `.scene` file and tracks every spawned entity in `owned`.
    bool load_scene(std::string_view path);
};

/// Returns the camera pose used by the Hill station (near mixed-LOD or `--far` coarse-LOD).
[[nodiscard]] astral::Vec3 hill_camera_eye(bool far_view) noexcept;

bool build_world_station(StationBuild& build);
bool build_range_station(StationBuild& build);
bool build_hill_station(StationBuild& build);

} // namespace world_showcase
