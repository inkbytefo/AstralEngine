#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    shader_showcase::ShowcaseGame game;
    return astral::run_game(options,
                            {.title = "Astral Shader Showcase",
                             .render = {.ray_tracing = !options.has("--raster")},
                             .asset_root = ASTRAL_GAME_ASSETS,
                             .pack = ASTRAL_GAME_PACK},
                            game);
}
