// Opens a scene file (.scene) and shows it. Prefab and scene files reload while it runs: edit
// assets/prefabs/box.prefab in a text editor and every box in the room changes.
//
//   scene_viewer [path/to/level.scene] [--debug] [--ui]      window (--debug: light ranges, axes, grid;
//                                                            --ui: debug UI open at start; F1 toggles it)
//   scene_viewer [scene] --capture out.ppm [--frames 48] [--width 1280] [--height 720]
//
// Without a path it opens the RGB room (showcase.scene): from assets/ while developing, from
// scene_viewer.apak once packaged (`cmake --build ... --target scene_viewer_package`).
#include "game.hpp"

#include <astral/app/shell.hpp>

#include <filesystem>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    // A scene given on the command line is read from its folder; otherwise the game's own assets.
    std::string root = ASTRAL_GAME_ASSETS, file = "showcase.scene";
    std::string_view pack = ASTRAL_GAME_PACK;
    if (const auto given = options.positional(); !given.empty()) {
        const std::filesystem::path scene = std::filesystem::absolute(given.front());
        root = scene.parent_path().string();
        file = scene.filename().string();
        pack = {};
    }
    astral::RenderSettings render;
    render.bloom_strength = 0.05f;
    scene_viewer::ViewerGame game(file, options.has("--debug"), options.has("--ui"));
    return astral::run_game(options, {.title = "Astral — scene viewer", .render = render, .asset_root = root, .pack = pack}, game, 48);
}
