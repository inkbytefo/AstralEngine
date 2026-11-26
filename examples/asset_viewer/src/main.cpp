// asset_viewer: opens glTF/GLB models and .scene files in one viewer with orbit/fly camera,
// live model/scene statistics HUD, hot reload, and cook/package support.
//
//   asset_viewer [path/to/file.scene|model.glb|model.gltf]
//                [--view scene|model] [--camera orbit|fly] [--debug] [--ui] [--raster] [--pack file.apak]
//                [--capture out.ppm --frames N --width W --height H]
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions shell = astral::parse_shell_args(argc, argv);
    const asset_viewer::ViewerOptions options =
        asset_viewer::resolve_launch(shell, ASTRAL_GAME_ASSETS, ASTRAL_GAME_PACK);
    asset_viewer::ViewerGame game(options);
    return astral::run_game(shell,
                            {.title = "Astral — asset viewer",
                             .render = game.settings(),
                             .asset_root = options.asset_root,
                             .pack = options.pack},
                            game, 48);
}
