#pragma once

#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>
#include <astral/world/world.hpp>

#include <string>
#include <string_view>

namespace astral {
class Context;

/// Optional tag lets script-based callers stop all effects with that tag.
struct PlayVfx {
    std::string asset;
    Vec3 position{};
    std::string tag;
};
struct StopVfx {
    std::string tag;
};
/// Clears gameplay smoke along a capsule. Equal endpoints make a sphere.
struct DisperseSmoke {
    Vec3 from{}, to{};
    float radius_m = 0.25f;
    std::uint32_t hold_steps = 15;
};

/// Plays a versioned .vfx asset at a local world position. Invalid on a load or validation error.
Entity play_vfx(Context& context, std::string_view asset, Vec3 position);
/// Cancels playback and removes its effects at the next App update.
void stop_vfx(Context& context, Entity instance);

} // namespace astral

ASTRAL_REFLECT(astral::PlayVfx, asset, position, tag)
ASTRAL_REFLECT(astral::StopVfx, tag)
ASTRAL_REFLECT(astral::DisperseSmoke, from, to, radius_m, hold_steps)
