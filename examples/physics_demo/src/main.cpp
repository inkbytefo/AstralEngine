// physics_demo — rigid bodies, raycasts and a character controller on AstralEngine.
// A box pyramid, balls raining down, left click shoots a ball from the camera, right click knocks back whatever
// the crosshair ray hits. WASD walk, Space jump, arrows turn, Esc quit.
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    physics_demo::PhysicsGame game;
    return astral::run_game(options, {.title = "Astral — physics demo", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game);
}
