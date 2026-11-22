// physics_playground (M12): a height-field terrain with a lake — light crates float, stones sink, balls drift in a gusty,
// turning wind —, a plank bridge on hinges, a jump pad (a trigger volume with a Luau rule) and a glider.
//   physics_playground                   WASD / Q / E fly, right mouse look, Shift fast, F1 debug panels
//   physics_playground --capture out.ppm [--frames N] [--width W] [--height H] [--overlays]   (--overlays: shapes, joints, fields)
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    physics_playground::PlaygroundGame game(options.has("--overlays"));
    astral::RenderSettings render;
    render.bloom_strength = 0.05f;
    return astral::run_game(options, {.title = "Astral — physics playground", .render = render, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK,
                                     .physics_interpolation = true}, game, 240);
}
