#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const auto options = astral::parse_shell_args(argc, argv);
    const astral::RenderSettings render{
        .ray_tracing = !options.has("--raster"),
        .gi = {.probes_x = 6, .probes_y = 4, .probes_z = 4},
        .path_tracer = options.has("--path-tracer"),
        .path_tracer_max_bounces = 4,
    };
    material_workflow::WorkflowGame game;
    return astral::run_game(options, {.title = "Astral Material Workflow", .render = render,
                                      .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 12);
}
