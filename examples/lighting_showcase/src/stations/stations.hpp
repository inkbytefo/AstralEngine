#pragma once
// The three stations. Only one is loaded at a time: moving SDF shapes reset the ray tracer's temporal history, and sun,
// environment and GI volume are global, so each station owns the whole picture while it is there.

#include <astral/astral.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lighting_showcase {

enum class StationId : std::uint8_t { Room, Yard, Night };

struct StationInfo {
    const char* name;  ///< Command line: --view <name>.
    const char* title; ///< HUD.
    const char* hint;  ///< HUD, one line: what to look at.
};

[[nodiscard]] const StationInfo& info(StationId id);
/// Unknown names mean the room.
[[nodiscard]] StationId station_from_name(std::string_view name);
/// Renderer settings that suit the station (GI volume, bloom).
[[nodiscard]] astral::RenderSettings station_settings(StationId id);

/// Render resources made once and used again when a station is rebuilt (the material service has no destroy).
/// A station asks for its materials in the same order every time.
class StationResources {
public:
    astral::MaterialHandle material(astral::Context& ctx, StationId station, const astral::MaterialDesc& desc);
    astral::MeshHandle smooth_sphere(astral::Context& ctx);
    /// Call before building a station.
    void rewind() noexcept { next_ = 0; }

private:
    std::vector<astral::MaterialHandle> materials_[3];
    std::size_t next_ = 0;
    astral::MeshHandle sphere_{};
};

/// What a builder works with. Everything it creates goes into `owned`, so leaving the station removes all of it.
/// Builders also set ctx.sun and ctx.environment completely.
struct StationBuild {
    astral::Context& ctx;
    StationResources& resources;
    std::vector<astral::Entity>& owned;
    bool raster = false; ///< No ray tracing: stand-ins for what only the ray-traced path draws.

    template <class... Cs> astral::Entity create(Cs&&... components) {
        const astral::Entity e = ctx.world.create(std::forward<Cs>(components)...);
        owned.push_back(e);
        return e;
    }
    astral::MaterialHandle material(StationId station, const astral::MaterialDesc& desc) {
        return resources.material(ctx, station, desc);
    }
    /// A fly camera at `eye` looking at `target`.
    astral::Entity camera(astral::Vec3 eye, astral::Vec3 target, float fov_degrees);
};

inline constexpr std::size_t night_light_count = 36;

void build_room(StationBuild& build);
void build_yard(StationBuild& build);
void build_night(StationBuild& build);

} // namespace lighting_showcase
