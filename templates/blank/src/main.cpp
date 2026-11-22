// The standard main() of an Astral game (see astral/app/shell.hpp for the command line):
//   blank                     play        blank --capture out.ppm   one offscreen image
//   blank --script-api docs   write astral.d.luau and API.md
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    blank::BlankGame game;
    return astral::run_game(options,
                            {.title = "Blank game", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK},
                            game);
}
