#pragma once
// Viewport ray math, deterministic CPU entity picking, selection bounds, and 3D transform gizmos (ADR-0037).

#include <astral/core/math.hpp>
#include <astral/editor/workspace.hpp>
#include <astral/render/debug_draw.hpp>
#include <astral/render/renderer.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/world.hpp>

#include <optional>

namespace astral::editor {

struct PickRay {
    Vec3 origin{};
    Vec3 dir{0.0f, 0.0f, -1.0f};
};

struct PickHit {
    Entity entity{};
    DocumentEntityId doc_id{};
    float distance = 0.0f;
};

struct GizmoDragState {
    bool active = false;
    int axis = -1; ///< 0=X, 1=Y, 2=Z
    DocumentEntityId target{};
    Vec3 start_position{};
    Quat start_rotation{};
    Vec3 start_scale{1.0f, 1.0f, 1.0f};
    Vec3 start_axis_dir{1.0f, 0.0f, 0.0f};
    float start_axis_t = 0.0f;
    Vec2 start_uv{};
    std::string coalesce_key;
};

/// Unprojects normalized viewport UV ([0,1]x[0,1], top-left origin) into a world-space ray.
[[nodiscard]] PickRay viewport_uv_to_ray(const CameraData& cam, Vec2 uv) noexcept;

/// Intersects a ray with an axis-aligned bounding box. Returns hit distance >= 0 or nullopt.
[[nodiscard]] std::optional<float> ray_aabb_hit(const PickRay& ray, Vec3 bmin, Vec3 bmax) noexcept;

/// Picks the closest visible entity (ignoring `ignore_camera`) along `ray`.
[[nodiscard]] std::optional<PickHit> pick_world_entity(World& world, const PickRay& ray, Entity ignore_camera);

/// Tests whether `ray` hits one of the 3 gizmo axes (0=X, 1=Y, 2=Z) around `origin`. Returns -1 if none.
[[nodiscard]] int pick_gizmo_axis(const PickRay& ray, Vec3 origin, Quat rot, TransformSpace space, float scale) noexcept;

/// Projects a ray onto a world-space axis line `(origin, axis_dir)` and returns the scalar parameter `t`.
[[nodiscard]] float project_ray_onto_axis(const PickRay& ray, Vec3 origin, Vec3 axis_dir) noexcept;

/// Draws the subtle ground grid, selection wireframe outline, and 3D transform gizmo into `debug`.
void draw_viewport_overlays(DebugDraw& debug, World& world, Entity selected, Vec3 cam_pos, bool show_grid,
                            GizmoMode mode, TransformSpace space, int active_axis);

/// Snaps `value` to multiples of `step` when `enabled` and `step > 0`.
[[nodiscard]] float apply_snap(float value, bool enabled, float step) noexcept;

} // namespace astral::editor
