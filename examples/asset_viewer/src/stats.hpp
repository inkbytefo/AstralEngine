#pragma once
// Collects world, scene, model and renderer statistics and draws the viewer HUD.

#include "options.hpp"

#include <astral/astral.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace asset_viewer {

struct ModelStats {
    std::size_t loaded_models = 0;
    std::size_t failed_models = 0;
    std::size_t nodes = 0;
    std::size_t pieces = 0;
    std::size_t vertices = 0;
    std::size_t triangles = 0;
    std::size_t materials = 0;
    std::size_t images = 0;
    std::size_t warnings = 0;
    astral::Vec3 bounds_size{};
    bool operator==(const ModelStats&) const = default;
};

struct ViewerStats {
    std::size_t entities = 0;
    std::size_t meshes = 0;
    std::size_t primitives = 0;
    std::size_t model_refs = 0;
    std::size_t prefab_instances = 0;
    std::size_t prefab_parts = 0;
    std::size_t sdf_shapes = 0;
    std::size_t rect_lights = 0;
    std::size_t point_lights = 0;
    std::size_t cameras = 0;
    std::uint32_t asset_reloads = 0;
    std::uint32_t scene_reloads = 0;
    bool from_pack = false;
    bool has_sun = false;
    ModelStats model{};
};

class StatsTracker {
public:
    void reset(std::filesystem::path asset_root);
    ViewerStats collect(astral::Context& ctx, std::uint32_t scene_reloads);

private:
    std::filesystem::path asset_root_;
    std::vector<std::string> cached_paths_;
    std::uint32_t cached_reloads_ = 0;
    bool cache_ready_ = false;
    ModelStats cached_model_{};
};

void draw_viewer_hud(astral::Context& ctx, const ViewerStats& stats, StationId station, std::string_view file,
                     CameraMode camera, bool debug_overlays, bool external, const astral::RenderSettings& render);

} // namespace asset_viewer
