// model_viewer — shows a glTF/GLB model in a small lit studio, with hot reload: re-export from Blender and the
// model updates in place.
//
//   model_viewer path/to/model.glb                 window
//   model_viewer path/to/model.glb --capture out.ppm [--frames 48] [--width 1280] [--height 720]
// A procedural test model: python3 tools/make_test_model.py crate.glb
#include "game.hpp"
#include "studio.hpp"

#include <astral/app/shell.hpp>

#include <cstdio>
#include <filesystem>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    const auto given = options.positional();
    if (given.empty()) {
        std::fprintf(stderr, "usage: model_viewer <model.gltf|glb> [--capture out.ppm --frames N --width W --height H]\n");
        return 1;
    }
    const std::filesystem::path file = std::filesystem::absolute(given.front());
    const std::string root = file.parent_path().string(); // the model's folder is the asset root
    model_viewer::ViewerGame game(file.filename().string());
    return astral::run_game(options, {.title = "Astral — model viewer", .render = model_viewer::studio_settings(), .asset_root = root}, game, 48);
}
