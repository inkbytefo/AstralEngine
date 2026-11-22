// 3D Pong: keep the ball in the tunnel. WASD / arrows or the mouse move your (blue) paddle; where
// the ball hits the paddle bends its path. First to 5. Esc quits.
//
//   pong                                   play
//   pong --demo                            the AI plays both sides
//   pong --script-api <folder>             also writes astral.d.luau and API.md (the script API)
//   pong --capture out.ppm [--frames N] [--width W] [--height H]   AI vs AI, one image
//
// Everything you see is assets/court.scene and its prefabs (edit them while it runs); the rules are
// Luau (assets/features/pong_rules/*.luau, reloaded when you save) and two small C++ features.

#include "game.hpp"

#include <astral/app/shell.hpp>


using namespace astral;

int main(int argc, char** argv) {
    const ShellOptions options = parse_shell_args(argc, argv);
    RenderSettings render;
    render.bloom_strength = 0.07f;
    pong::PongGame game(options.has("--demo") || options.capturing());
    return run_game(options, {.title = "Astral Pong", .render = render, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 150);
}
