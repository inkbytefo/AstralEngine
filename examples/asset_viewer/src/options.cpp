#include "options.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace asset_viewer {

namespace {

std::string lowercase_extension(std::string_view path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

StationId station_from_name(std::string_view name) noexcept {
    return name == "model" ? StationId::Model : StationId::Scene;
}

CameraMode camera_mode_from_name(std::string_view name) noexcept {
    return name == "fly" ? CameraMode::Fly : CameraMode::Orbit;
}

bool is_model_extension(std::string_view path) noexcept {
    const std::string ext = lowercase_extension(path);
    return ext == ".glb" || ext == ".gltf";
}

std::string_view default_file_for(StationId station) noexcept {
    return station == StationId::Model ? "props/crate.glb" : "showcase.scene";
}

ViewerOptions resolve_launch(const astral::ShellOptions& shell, std::string_view builtin_assets,
                             std::string_view builtin_pack) {
    ViewerOptions opts{};
    opts.debug = shell.has("--debug");
    opts.ui = shell.has("--ui");
    opts.raster = shell.has("--raster");
    opts.camera_mode = camera_mode_from_name(shell.value("--camera", "orbit"));
    opts.fallback_assets = std::string(builtin_assets);
    opts.fallback_pack = std::string(builtin_pack);

    const std::string explicit_view = shell.value("--view", "");
    const std::string explicit_pack = shell.value("--pack", "");
    const auto positional = shell.positional({"--view", "--camera", "--pack"});

    if (!positional.empty()) {
        const std::filesystem::path given(positional.front());
        std::error_code ec;
        const bool on_disk = std::filesystem::is_regular_file(given, ec) || given.is_absolute();
        if (on_disk) {
            const std::filesystem::path abs = std::filesystem::absolute(given, ec);
            opts.asset_root = abs.parent_path().string();
            opts.file = abs.filename().string();
            opts.pack = explicit_pack;
            opts.external = true;
        } else {
            opts.asset_root = std::string(builtin_assets);
            opts.pack = explicit_pack.empty() ? std::string(builtin_pack) : explicit_pack;
            opts.file = positional.front();
            opts.external = false;
        }
        opts.station = explicit_view.empty() ? (is_model_extension(opts.file) ? StationId::Model : StationId::Scene)
                                             : station_from_name(explicit_view);
        return opts;
    }

    opts.station = station_from_name(explicit_view.empty() ? "scene" : explicit_view);
    opts.file = std::string(default_file_for(opts.station));
    opts.asset_root = std::string(builtin_assets);
    opts.pack = explicit_pack.empty() ? std::string(builtin_pack) : explicit_pack;
    opts.external = false;
    return opts;
}

} // namespace asset_viewer
