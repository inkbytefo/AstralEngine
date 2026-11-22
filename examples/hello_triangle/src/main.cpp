// hello_triangle — the smallest picture: a mesh made from three vertices and an unlit .ashader material.
//   hello_triangle                      window
//   hello_triangle --capture out.ppm    one image
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    hello_triangle::TriangleGame game;
    return astral::run_game(options, {.title = "Astral — hello triangle", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game, 8);
}
