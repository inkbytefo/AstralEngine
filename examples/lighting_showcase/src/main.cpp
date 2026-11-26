// lighting_showcase: the lighting of the engine in three stations, one loaded at a time: a ray-traced RGB room lit by
// one panel, an SDF yard under the sun with orbiting lights, and a night with many lights (ReSTIR).
//   lighting_showcase                  1/2/3 stations, P path tracer, R ReSTIR, V debug view, WASD+QE fly, right mouse look
//   lighting_showcase --capture out.ppm [--frames N] [--width W] [--height H] [--view room|yard|night]
//                     [--path-tracer] [--raster] [--debug-view 0..5]   (albedo, normals, diffuse, specular, motion)
#include "game.hpp"

#include <astral/app/shell.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    const int debug_view = std::atoi(options.value("--debug-view", "0").c_str());
    lighting_showcase::ShowcaseGame game({.station = lighting_showcase::station_from_name(options.value("--view", "room")),
                                          .raster = options.has("--raster"),
                                          .path_tracer = options.has("--path-tracer"),
                                          .debug_view = static_cast<astral::DebugView>(((debug_view % 6) + 6) % 6)});
    return astral::run_game(options, {.title = "Astral — lighting showcase", .render = game.settings(),
                                      .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK},
                            game, 64);
}
