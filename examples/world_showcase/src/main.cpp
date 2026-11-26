// world_showcase: open-world streaming, floating origin, terrain layers & LOD, and day/night in three stations.
//   world_showcase                1/2/3 stations, N day/night, C map cells, F hill far, WASD+QE fly, right mouse look
//   world_showcase --capture out.ppm [--frames N] [--width W] [--height H]
//                  [--view world|range|hill] [--night] [--nudge metres] [--far] [--raster] [--cells]
#include "game.hpp"

#include <astral/app/shell.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
    const astral::ShellOptions shell = astral::parse_shell_args(argc, argv);
    const bool far_view = shell.has("--far");
    const std::string view_name = shell.value("--view", "");
    world_showcase::ShowcaseGame game({
        .station = world_showcase::resolve_initial_station(view_name, far_view),
        .capturing = shell.capturing(),
        .night = shell.has("--night"),
        .far_view = far_view,
        .raster = shell.has("--raster"),
        .show_cells = shell.has("--cells"),
        .nudge = std::strtof(shell.value("--nudge", "0").c_str(), nullptr),
    });
    return astral::run_game(shell,
                            {.title = "Astral — world showcase",
                             .render = game.render_settings(),
                             .asset_root = ASTRAL_GAME_ASSETS,
                             .pack = ASTRAL_GAME_PACK},
                            game, 90);
}
