#pragma once

#include <astral/core/math.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

/// Draws a mesh at the entity's WorldTransform.
struct MeshRenderer {
    MeshHandle mesh;
    Vec4 color{1, 1, 1, 1};
};

/// Turns the entity into the camera (its WorldTransform is the camera pose; looks down -Z).
/// The first active camera found is used.
struct Camera {
    float fov_y = radians(70.0f);
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    bool active = true;
};

/// Frame timing, available to every system as ctx.time.
struct Time {
    float delta = 0.0f;       ///< Seconds since the previous frame (clamped).
    double elapsed = 0.0;     ///< Seconds since the app started.
    std::uint64_t frame = 0;  ///< Frames completed.
};

} // namespace astral
