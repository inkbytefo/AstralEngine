#pragma once
// Lit model studio (ported from model_viewer): floor, key area light, sun, environment, camera and GI volume.

#include <astral/astral.hpp>

#include <string_view>
#include <vector>

namespace asset_viewer {

struct Studio {
    astral::Vec3 eye{2.3f, 1.7f, 2.8f};
    astral::Vec3 target{0.0f, 0.55f, 0.0f};
    astral::RectLight key{.position = {-1.5f, 3.0f, 1.5f},
                          .right = {0.6f, 0.0f, 0.6f},
                          .up = {-0.35f, 0.5f, 0.35f},
                          .color = {1.0f, 0.95f, 0.88f},
                          .intensity = 14.0f};
    astral::DirectionalLight sun{.direction = {0.4f, -1.0f, -0.6f},
                                 .color = {1.0f, 0.97f, 0.92f},
                                 .intensity = 1.2f,
                                 .angular_radius = 0.02f};
    astral::Environment environment{.sky = {0.35f, 0.42f, 0.52f}, .ground = {0.12f, 0.11f, 0.1f}, .exposure = 1.0f};
};

[[nodiscard]] astral::RenderSettings studio_settings();
[[nodiscard]] astral::RenderSettings scene_settings();

/// Spawns the model, studio floor, key light (plus raster stand-in if needed), camera, and sets sun/environment.
astral::Entity build_model_studio(astral::Context& ctx, std::string_view model_file, bool raster,
                                  astral::MaterialHandle& floor_material, std::vector<astral::Entity>& owned);

} // namespace asset_viewer
