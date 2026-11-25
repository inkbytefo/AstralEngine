#include <astral/app/shell.hpp>
#include <astral/editor/workspace.hpp>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Minimal uncompressed PPM writer for zero-dependency offscreen visual verification captures.
bool write_ppm(const std::string& path, std::uint32_t w, std::uint32_t h, const std::vector<std::uint8_t>& rgba) {
    const std::size_t pixels = std::size_t{w} * h;
    if (rgba.size() < pixels * 4u) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    std::vector<char> rgb(pixels * 3u);
    for (std::size_t i = 0; i < pixels; ++i) {
        rgb[i * 3u + 0u] = static_cast<char>(rgba[i * 4u + 0u]);
        rgb[i * 3u + 1u] = static_cast<char>(rgba[i * 4u + 1u]);
        rgb[i * 3u + 2u] = static_cast<char>(rgba[i * 4u + 2u]);
    }
    out << "P6\n" << w << " " << h << "\n255\n";
    if (!rgb.empty()) out.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
    return static_cast<bool>(out);
}

} // namespace

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    const std::string& capture_path = options.capture;
    const astral::editor::WorkspacePreset preset =
        options.has("--advanced") ? astral::editor::WorkspacePreset::Advanced : astral::editor::WorkspacePreset::Focus;
    const std::uint32_t width = options.width ? options.width : 1600u;
    const std::uint32_t height = options.height ? options.height : 900u;

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
