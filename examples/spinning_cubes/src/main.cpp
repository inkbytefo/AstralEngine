// spinning_cubes — what a game looks like on AstralEngine: components are plain structs, rules are systems
// (the spin in C++, running on every worker thread; the bobbing and the camera in Luau), the App runs the loop.
// WASD fly, Q/E down/up, Shift fast, hold the right mouse button to look, Esc quits.
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    spinning_cubes::CubesGame game;
    return astral::run_game(options, {.title = "Astral — spinning cubes", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game);
}
