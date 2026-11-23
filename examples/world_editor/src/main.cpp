#include <astral/editor/workspace.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// Minimal uncompressed PPM writer for zero-dependency offscreen visual verification captures.
bool write_ppm(const std::string& path, std::uint32_t w, std::uint32_t h, const std::vector<std::uint8_t>& rgba) {
    if (rgba.size() < std::size_t{w} * h * 4u) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "P6\n" << w << " " << h << "\n255\n";
    for (std::size_t i = 0; i < std::size_t{w} * h; ++i) {
        out.put(static_cast<char>(rgba[i * 4 + 0]));
        out.put(static_cast<char>(rgba[i * 4 + 1]));
        out.put(static_cast<char>(rgba[i * 4 + 2]));
    }
    return static_cast<bool>(out);
}

} // namespace

int main(int argc, char** argv) {
    std::string capture_path;
    astral::editor::WorkspacePreset preset = astral::editor::WorkspacePreset::Focus;
    std::uint32_t width = 1600;
    std::uint32_t height = 900;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--capture" && i + 1 < argc) capture_path = argv[++i];
        else if (arg == "--advanced") preset = astral::editor::WorkspacePreset::Advanced;
        else if (arg == "--width" && i + 1 < argc) width = static_cast<std::uint32_t>(std::stoi(argv[++i]));
        else if (arg == "--height" && i + 1 < argc) height = static_cast<std::uint32_t>(std::stoi(argv[++i]));
    }

    const bool offscreen = !capture_path.empty();
    astral::App app({
        .title = "Astral Editor",
        .width = width,
        .height = height,
        .fixed_timestep = offscreen ? (1.0f / 60.0f) : 0.0f,
        .asset_root = ASTRAL_EDITOR_ASSETS,
        .offscreen = offscreen,
        .debug_ui = true,
        .debug_ui_builtin_panels = false,
        .debug_ui_ini_path = offscreen ? "" : "astral_editor_layout.ini",
    });
    astral::editor::EditorWorkspace editor({
        .asset_root = ASTRAL_EDITOR_ASSETS,
        .initial_document = "scenes/workshop.scene",
        .project_name = "Astral Workshop",
        .initial_preset = preset,
        .manual_save = true,
    });
    if (offscreen) {
        app.run_frames(editor, 4);
        std::vector<std::uint8_t> rgba;
        if (!app.read_frame(rgba)) return 1;
        return write_ppm(capture_path, width, height, rgba) ? 0 : 1;
    }
    return app.run(editor);
}
