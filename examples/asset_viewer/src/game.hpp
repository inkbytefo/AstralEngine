#pragma once
// asset_viewer: one viewer for glTF/GLB models and scene files, with orbit/fly camera,
// live model/scene statistics HUD, hot reload, and cook/package support.

#include "options.hpp"
#include "stats.hpp"

#include <astral/astral.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asset_viewer {

bool install_features(astral::Context& ctx, std::string_view fallback_assets = {},
                      std::string_view fallback_pack = {});

class ViewerGame final : public astral::Game {
public:
    explicit ViewerGame(ViewerOptions options = {});
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

    [[nodiscard]] StationId station() const noexcept { return station_; }
    [[nodiscard]] CameraMode camera_mode() const noexcept { return camera_mode_; }
    [[nodiscard]] bool debug_overlays() const noexcept { return debug_; }
    [[nodiscard]] std::uint32_t scene_reloads() const noexcept { return scene_reloads_; }
    [[nodiscard]] const ViewerStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const astral::RenderSettings& settings() const noexcept { return settings_; }

private:
    void open_view(astral::Context& ctx, StationId id, std::string file);
    void load_scene_view(astral::Context& ctx, const std::string& file);
    void load_model_view(astral::Context& ctx, const std::string& file);
    void clear_owned(astral::Context& ctx);
    void set_camera_mode(astral::Context& ctx, CameraMode mode);
    void reset_camera(astral::Context& ctx);
    void apply_debug_flags(astral::Context& ctx);
    void check_scene_hot_reload(astral::Context& ctx);

    ViewerOptions options_;
    StationId station_ = StationId::Scene;
    CameraMode camera_mode_ = CameraMode::Orbit;
    bool debug_ = false;
    std::string active_file_;
    astral::RenderSettings settings_{};
    astral::MaterialHandle floor_material_{};
    std::vector<astral::Entity> owned_;
    astral::Entity camera_{};
    astral::Vec3 home_eye_{};
    astral::Vec3 home_target_{};
    astral::AssetId doc_id_{};
    std::uint32_t doc_version_ = 0;
    std::uint32_t scene_reloads_ = 0;
    StatsTracker tracker_;
    ViewerStats stats_{};
};

} // namespace asset_viewer
