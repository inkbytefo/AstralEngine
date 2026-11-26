#pragma once
// The world of the showcase, built in parts. The terrain is a basin with a lake in the middle (water up to y = 0) and a
// flat plateau at y = plateau_height from r = 28 m outwards; the stations stand on the plateau.

#include "game.hpp"

#include <astral/astral.hpp>

namespace physics_showcase {

inline constexpr float plateau_height = 3.0f;
/// Station centres on the plateau (x, z).
inline constexpr astral::Vec3 pile_centre{0, plateau_height, 40};
inline constexpr astral::Vec3 joints_centre{0, plateau_height, -40};
inline constexpr astral::Vec3 surfaces_centre{40, plateau_height, 0};
inline constexpr astral::Vec3 water_centre{0, 0, 0};

/// Height of the terrain at x, z (metres).
[[nodiscard]] float terrain_height(float x, float z);

void build_terrain_and_lake(astral::Context& ctx);   // terrain.cpp
void build_pile_station(astral::Context& ctx);       // pile.cpp: crate pyramid, rain, the player
void build_joints_station(astral::Context& ctx, Handles& h);    // joints.cpp
void build_surfaces_station(astral::Context& ctx, Handles& h);  // surfaces.cpp
void build_water_station(astral::Context& ctx);      // water.cpp: floaters, bridge, jump pad, glider
void build_player(astral::Context& ctx, Handles& h); // pile.cpp

/// A named place for the HUD.
void add_station(astral::Context& ctx, astral::Vec3 centre, const char* title, const char* hint, float radius);

} // namespace physics_showcase
