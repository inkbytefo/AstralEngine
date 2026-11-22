#pragma once
// A small lit studio: a floor, a key light, a sun and a camera looking at the origin.

#include <astral/astral.hpp>

namespace model_viewer {

struct Studio {
    astral::Vec3 eye{2.3f, 1.7f, 2.8f};
    astral::Vec3 target{0, 0.55f, 0};
    astral::RectLight key{.position = {-1.5f, 3.0f, 1.5f}, .right = {0.6f, 0, 0.6f}, .up = {-0.35f, 0.5f, 0.35f},
                          .color = {1.0f, 0.95f, 0.88f}, .intensity = 14.0f};
    astral::DirectionalLight sun{.direction = {0.4f, -1.0f, -0.6f}, .color = {1.0f, 0.97f, 0.92f}, .intensity = 1.2f, .angular_radius = 0.02f};
    astral::Environment environment{.sky = {0.35f, 0.42f, 0.52f}, .ground = {0.12f, 0.11f, 0.1f}, .exposure = 1.0f};
};

/// Renderer settings for the studio (a small GI volume around the model).
[[nodiscard]] astral::RenderSettings studio_settings();

} // namespace model_viewer
