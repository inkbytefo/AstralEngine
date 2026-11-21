#pragma once
// Immediate-mode debug lines: collect shapes during a frame, hand them to the renderer in
// RenderScene::debug_lines, clear. Plain data (no GPU): usable from any system, also headless.
//
//     debug.box(center, half, rotation, {1, 0, 0, 1});
//     debug.arrow(from, to, {0, 1, 0, 1}, {.on_top = true});
//     debug.sphere(p, 0.5f, color, {.seconds = 2});    // stays for two seconds
//     renderer.render(cmd, image, camera, {.meshes = ..., .debug_lines = debug.lines()});
//     debug.advance(dt);                                 // drops expired shapes

#include <astral/core/math.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace astral {

struct DebugLine {
    Vec3 a{};
    Vec3 b{};
    Vec4 color{1, 1, 1, 1};
    bool on_top = false; ///< Drawn over everything (otherwise hidden behind scene meshes).
};

struct DebugStyle {
    bool on_top = false;
    float seconds = 0.0f; ///< 0 = this frame only.
};

class DebugDraw {
public:
    void line(Vec3 a, Vec3 b, Vec4 color, DebugStyle style = {});
    /// Oriented box: centre, half extents, rotation.
    void box(Vec3 center, Vec3 half, Quat rotation, Vec4 color, DebugStyle style = {});
    void aabb(Vec3 min, Vec3 max, Vec4 color, DebugStyle style = {});
    /// Three great circles.
    void sphere(Vec3 center, float radius, Vec4 color, DebugStyle style = {}, int segments = 24);
    /// Vertical (local Y) capsule: cylinder part of `half_height` with hemispherical caps.
    void capsule(Vec3 center, Quat rotation, float radius, float half_height, Vec4 color, DebugStyle style = {});
    void cylinder(Vec3 center, Quat rotation, float radius, float half_height, Vec4 color, DebugStyle style = {});
    void circle(Vec3 center, Vec3 normal, float radius, Vec4 color, DebugStyle style = {}, int segments = 24);
    void arrow(Vec3 from, Vec3 to, Vec4 color, DebugStyle style = {});
    /// X (red), Y (green), Z (blue) axes of a transform.
    void axes(const Mat4& transform, float size = 1.0f, DebugStyle style = {});
    void cross(Vec3 point, float size, Vec4 color, DebugStyle style = {});
    /// Square grid on the XZ plane centred at `center`.
    void grid(Vec3 center, float size, int cells, Vec4 color, DebugStyle style = {});

    /// Every line to draw this frame.
    [[nodiscard]] std::span<const DebugLine> lines() const noexcept { return lines_; }
    /// Ends the frame: forgets this-frame shapes and those whose time ran out.
    void advance(float seconds);
    void clear() noexcept;

private:
    std::vector<DebugLine> lines_;
    std::vector<float> remaining_; ///< Per line: seconds left (0 = this frame only).
};

} // namespace astral
