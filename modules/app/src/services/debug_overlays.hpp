#pragma once
// Internal to Astral::App: draws physics, joint, field, light, and world-cell debug overlays.

#include <astral/app/app.hpp>

namespace astral {

class DebugDraw;
class World;
struct WorldCellsOverlay;

void draw_debug_overlays(World& world, const Context::DebugShow& show, DebugDraw& debug,
                         const WorldCellsOverlay* world_cells);

} // namespace astral
