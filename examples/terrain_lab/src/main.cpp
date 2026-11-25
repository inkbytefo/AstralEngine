#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const auto options = astral::parse_shell_args(argc, argv);
    terrain_lab::TerrainLabGame game(options.has("--far"));
    return astral::run_game(options,
                            {.title = "Astral Terrain Lab",
                             .render = {.ray_tracing = false},
                             .asset_root = ASTRAL_GAME_ASSETS,
                             .pack = ASTRAL_GAME_PACK},
                            game, 2);
}
