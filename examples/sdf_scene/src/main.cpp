// sdf_scene — the hybrid renderer: meshes and signed-distance-field shapes in one lit scene.
// SDF blobs melt into each other (smooth union), a box is carved by a sphere (subtract), coloured point
// lights orbit (Luau rule), the sun casts shadows from both meshes and SDFs.
#include "game.hpp"

#include <astral/app/shell.hpp>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    sdf_scene::SdfGame game;
    return astral::run_game(options, {.title = "Astral — SDF scene", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game);
}
