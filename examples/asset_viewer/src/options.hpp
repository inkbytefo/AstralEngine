#pragma once
// Command-line options and launch resolution for asset_viewer: chooses between a .scene and a glTF/GLB
// model (built-in stations or external files on disk), plus camera, debug, UI, raster and pack options.

#include <astral/app/shell.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace asset_viewer {

enum class StationId : std::uint8_t { Scene, Model };
enum class CameraMode : std::uint8_t { Orbit, Fly };

struct ViewerOptions {
    StationId station = StationId::Scene;
    std::string file;            ///< Relative asset path ("showcase.scene", "props/crate.glb", or external filename).
    std::string asset_root;      ///< AssetManager root folder.
    std::string pack;            ///< .apak path (empty for external files).
    std::string fallback_assets; ///< Built-in assets folder for loading viewer features in external-file mode.
    std::string fallback_pack;   ///< Built-in .apak name for loading viewer features in external-file mode.
    bool external = false;       ///< True when viewing a user-supplied file outside the built-in assets.
    bool debug = false;          ///< --debug: light ranges, physics shapes, grid and entity axes.
    bool ui = false;             ///< --ui: open the F1 debug UI at start.
    bool raster = false;         ///< --raster: disable ray tracing.
    CameraMode camera_mode = CameraMode::Orbit;
};

[[nodiscard]] StationId station_from_name(std::string_view name) noexcept;
[[nodiscard]] CameraMode camera_mode_from_name(std::string_view name) noexcept;
[[nodiscard]] bool is_model_extension(std::string_view path) noexcept;
[[nodiscard]] std::string_view default_file_for(StationId station) noexcept;

[[nodiscard]] ViewerOptions resolve_launch(const astral::ShellOptions& shell, std::string_view builtin_assets,
                                           std::string_view builtin_pack);

} // namespace asset_viewer
