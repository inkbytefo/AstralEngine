#include "viewport/viewport_math.hpp"

#include <astral/world/transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace astral::editor {

namespace {

Vec3 axis_direction(int axis, Quat rot, TransformSpace space) noexcept {
    const Vec3 base = axis == 0 ? Vec3{1, 0, 0} : (axis == 1 ? Vec3{0, 1, 0} : Vec3{0, 0, 1});
    return space == TransformSpace::Local ? normalize(rot * base) : base;
}

float ray_segment_distance(const PickRay& ray, Vec3 a, Vec3 b, float& out_s) noexcept {
    const Vec3 u = ray.dir;
    const Vec3 v = b - a;
    const Vec3 w = ray.origin - a;
    const float a_dot = dot(u, u);
    const float b_dot = dot(u, v);
    const float c_dot = dot(v, v);
    const float d_dot = dot(u, w);
    const float e_dot = dot(v, w);
    const float denom = a_dot * c_dot - b_dot * b_dot;
    float s = 0.0f;
    float t = 0.0f;
    if (denom > 1e-6f) {
        s = std::max(0.0f, (b_dot * e_dot - c_dot * d_dot) / denom);
        t = std::clamp((a_dot * e_dot - b_dot * d_dot) / denom, 0.0f, 1.0f);
    }
    const Vec3 p1 = ray.origin + u * s;
    const Vec3 p2 = a + v * t;
    out_s = s;
    return length(p1 - p2);
}

void entity_world_bounds(const Mat4& m, Vec3& out_min, Vec3& out_max) noexcept {
    const Vec3 center{m[3].x, m[3].y, m[3].z};
    const Vec3 ex{std::abs(m[0].x) + std::abs(m[1].x) + std::abs(m[2].x),
                  std::abs(m[0].y) + std::abs(m[1].y) + std::abs(m[2].y),
                  std::abs(m[0].z) + std::abs(m[1].z) + std::abs(m[2].z)};
    const Vec3 half{std::max(ex.x * 0.5f, 0.25f), std::max(ex.y * 0.5f, 0.25f), std::max(ex.z * 0.5f, 0.25f)};
    out_min = center - half;
    out_max = center + half;
}

} // namespace

PickRay viewport_uv_to_ray(const CameraData& cam, Vec2 uv) noexcept {
    const float ndc_x = uv.x * 2.0f - 1.0f;
    const float ndc_y = uv.y * 2.0f - 1.0f; // Vulkan projection has Y-down clip space
    const Mat4 inv_vp = inverse(cam.projection * cam.view);
    Vec4 near_p = inv_vp * Vec4{ndc_x, ndc_y, 0.0f, 1.0f};
    Vec4 far_p = inv_vp * Vec4{ndc_x, ndc_y, 1.0f, 1.0f};
    if (std::abs(near_p.w) > 1e-6f) near_p = near_p * (1.0f / near_p.w);
    if (std::abs(far_p.w) > 1e-6f) far_p = far_p * (1.0f / far_p.w);
    const Vec3 o{near_p.x, near_p.y, near_p.z};
    const Vec3 f{far_p.x, far_p.y, far_p.z};
    const Vec3 d = f - o;
    const float len = length(d);
    return {o, len > 1e-6f ? (d * (1.0f / len)) : Vec3{0, 0, -1}};
}

std::optional<float> ray_aabb_hit(const PickRay& ray, Vec3 bmin, Vec3 bmax) noexcept {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    const float orig[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const float lo[3] = {bmin.x, bmin.y, bmin.z};
    const float hi[3] = {bmax.x, bmax.y, bmax.z};
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-7f) {
            if (orig[i] < lo[i] || orig[i] > hi[i]) return std::nullopt;
            continue;
        }
        const float inv = 1.0f / dir[i];
        float t1 = (lo[i] - orig[i]) * inv;
        float t2 = (hi[i] - orig[i]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return std::nullopt;
    }
    return tmin;
}

std::optional<PickHit> pick_world_entity(World& world, const PickRay& ray, Entity ignore_camera) {
    std::optional<PickHit> best;
    world.each<const WorldTransform>([&](Entity e, const WorldTransform& wt) {
        if (e == ignore_camera) return;
        const SceneId* sid = world.get<const SceneId>(e);
        if (!sid || sid->value == 0) return;
        Vec3 bmin{}, bmax{};
        entity_world_bounds(wt.matrix, bmin, bmax);
        const auto hit = ray_aabb_hit(ray, bmin, bmax);
        if (!hit) return;
        if (!best || *hit < best->distance) {
            best = PickHit{.entity = e, .doc_id = {sid->value, {}}, .distance = *hit};
        }
    });
    return best;
}

int pick_gizmo_axis(const PickRay& ray, Vec3 origin, Quat rot, TransformSpace space, float scale) noexcept {
    int best_axis = -1;
    float best_dist = scale * 0.16f;
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dir = axis_direction(axis, rot, space);
        float s = 0.0f;
        const float dist = ray_segment_distance(ray, origin, origin + dir * scale, s);
        if (dist < best_dist) {
            best_dist = dist;
            best_axis = axis;
        }
    }
    return best_axis;
}

float project_ray_onto_axis(const PickRay& ray, Vec3 origin, Vec3 axis_dir) noexcept {
    const Vec3 w = ray.origin - origin;
    const float a = dot(axis_dir, axis_dir);
    const float b = dot(axis_dir, ray.dir);
    const float c = dot(ray.dir, ray.dir);
    const float d = dot(axis_dir, w);
    const float e = dot(ray.dir, w);
    const float denom = a * c - b * b;
    if (std::abs(denom) < 1e-6f) return 0.0f;
    return (c * d - b * e) / denom;
}

void draw_viewport_overlays(DebugDraw& debug, World& world, Entity selected, Vec3 cam_pos, bool show_grid,
                            GizmoMode mode, TransformSpace space, int active_axis) {
    if (show_grid) {
        debug.grid({0.0f, 0.0f, 0.0f}, 24.0f, 24, {0.32f, 0.34f, 0.40f, 0.35f});
    }
    if (!selected.valid() || !world.alive(selected)) return;
    const WorldTransform* wt = world.get<const WorldTransform>(selected);
    if (!wt) return;

    Vec3 bmin{}, bmax{};
    entity_world_bounds(wt->matrix, bmin, bmax);
    const Vec3 center = (bmin + bmax) * 0.5f;
    const Vec3 half = (bmax - bmin) * 0.5f;
    // Muted violet selection outline (on top for crisp readability without neon glow)
    debug.box(center, half, Quat{}, {0.58f, 0.52f, 0.78f, 0.95f}, {.on_top = true});

    if (mode == GizmoMode::Select) return;
    const Vec3 origin{wt->matrix[3].x, wt->matrix[3].y, wt->matrix[3].z};
    const Quat rot = rotation_of(wt->matrix);
    const float dist = std::max(length(cam_pos - origin), 1.5f);
    const float gscale = dist * 0.16f;
    const Vec4 cols[3] = {{0.86f, 0.32f, 0.34f, 1.0f}, {0.36f, 0.78f, 0.46f, 1.0f}, {0.34f, 0.56f, 0.88f, 1.0f}};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dir = axis_direction(axis, rot, space);
        const Vec4 col = (active_axis == axis) ? Vec4{0.96f, 0.88f, 0.42f, 1.0f} : cols[axis];
        if (mode == GizmoMode::Rotate) {
            debug.circle(origin, dir, gscale * 0.85f, col, {.on_top = true});
        } else if (mode == GizmoMode::Scale) {
            const Vec3 tip = origin + dir * gscale;
            debug.line(origin, tip, col, {.on_top = true});
            debug.box(tip, Vec3{gscale * 0.08f, gscale * 0.08f, gscale * 0.08f}, rot, col, {.on_top = true});
        } else {
            debug.arrow(origin, origin + dir * gscale, col, {.on_top = true});
        }
    }
}

float apply_snap(float value, bool enabled, float step) noexcept {
    if (!enabled || step <= 1e-5f) return value;
    return std::round(value / step) * step;
}

} // namespace astral::editor
