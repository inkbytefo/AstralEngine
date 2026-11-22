// Open world demo (M9, ADR-0021): fly over a generated 4 x 4 km map whose cells stream in and out around the camera,
// with a floating origin and a "night" data layer.
//
//   open_world                      fly: WASD, Q/E down/up, Shift fast, right mouse look, N night, F1 panels
//   open_world --capture out.ppm [--frames 40] [--width 1280] [--height 720] [--night] [--nudge metres]
#include "game.hpp"

#include <astral/app/shell.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    open_world::OpenWorldGame game(options.capturing(), options.has("--night"), std::strtof(options.value("--nudge", "0").c_str(), nullptr));
    return astral::run_game(options, {.title = "Astral Open World", .asset_root = ASTRAL_OPEN_WORLD_ASSETS}, game, 40);
}
