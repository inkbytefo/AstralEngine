// materials_showcase: PBR shading models, .material workflow and .ashader night gallery in three stations.
//   materials_showcase                      1/2/3 stations, P path tracer, U UV/density, WASD+QE fly, right mouse look
//   materials_showcase --capture out.ppm [--frames N] [--width W] [--height H] [--view studio|workflow|gallery]
//                      [--path-tracer] [--raster] [--uv-inspect] [--uv-density]
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    materials_showcase::UvInspectMode uv_mode = materials_showcase::UvInspectMode::Off;
    if (options.has("--uv-density")) uv_mode = materials_showcase::UvInspectMode::Density;
    else if (options.has("--uv-inspect")) uv_mode = materials_showcase::UvInspectMode::Checker;

    materials_showcase::ShowcaseGame game({
        .station = materials_showcase::station_from_name(options.value("--view", "studio")),
        .raster = options.has("--raster"),
        .path_tracer = options.has("--path-tracer"),
        .uv_mode = uv_mode,
    });
    return astral::run_game(options, {.title = "Astral — materials showcase", .render = game.settings(),
                                      .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK},
                            game, 64);
}
