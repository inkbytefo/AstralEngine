// physics_lab (P0 — ADR-0038): motorized SixDOF/SwingTwist/Hinge joints, breakable joints,
// physics material table, cooked shapes, batched raycasts, and debug joint overlays.
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    physics_lab::PhysicsLabGame game(options.has("--overlays"));
    astral::RenderSettings render;
    render.bloom_strength = 0.05f;
    return astral::run_game(options, {.title = "Astral — physics lab (P0)", .render = render,
                                      .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK,
                                      .physics_interpolation = true},
                            game, 180);
}
