// rt_showcase — the ray-traced renderer (ADR-0011) in a closed "RGB room": red / green / blue walls, a mirror-chrome
// sphere, glossy and rough boxes, a gold SDF torus, and a single ceiling panel light. Everything beyond the panel's
// direct light — colour bleeding, soft contact shadows, the room in the sphere — comes from ray tracing.
//
//   rt_showcase                         window (needs a ray-tracing GPU; falls back to raster)
//   rt_showcase --capture out.ppm [--frames 64] [--width 1280] [--height 720] [--view 0..2]
//               [--debug-view 0..4] [--raster]        (debug views: albedo, normals, diffuse light, specular)
#include "game.hpp"
#include "room.hpp"

#include <astral/app/shell.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    astral::RenderSettings render = rt_showcase::showcase_settings();
    render.debug_view = static_cast<astral::DebugView>(std::atoi(options.value("--debug-view", "0").c_str()) % 5);
    render.ray_tracing = !options.has("--raster");
    rt_showcase::ShowcaseGame game(std::atoi(options.value("--view", "0").c_str()));
    return astral::run_game(options, {.title = "Astral — RT showcase", .render = render}, game, 64);
}
