#include <astral/render/debug_draw.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace astral {

namespace {
Vec3 rotate_vec(Quat q, Vec3 v) {
    const Vec4 r = rotate(q) * Vec4{v.x, v.y, v.z, 0};
    return {r.x, r.y, r.z};
}
/// Two unit vectors perpendicular to `n` and to each other.
void basis(Vec3 n, Vec3& u, Vec3& v) {
    n = normalize(n);
    const Vec3 helper = std::abs(n.y) < 0.99f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    u = normalize(cross(helper, n));
    v = cross(n, u);
}
} // namespace

void DebugDraw::line(Vec3 a, Vec3 b, Vec4 color, DebugStyle style) {
    lines_.push_back({a, b, color, style.on_top});
    remaining_.push_back(style.seconds);
}

void DebugDraw::box(Vec3 c, Vec3 h, Quat q, Vec4 color, DebugStyle style) {
    Vec3 corner[8];
    for (int i = 0; i < 8; ++i) {
        const Vec3 local{(i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z};
        corner[i] = c + rotate_vec(q, local);
    }
    static constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                         {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : edges) line(corner[e[0]], corner[e[1]], color, style);
}

void DebugDraw::aabb(Vec3 min, Vec3 max, Vec4 color, DebugStyle style) {
    box((min + max) * 0.5f, (max - min) * 0.5f, Quat{}, color, style);
}

void DebugDraw::circle(Vec3 center, Vec3 normal, float radius, Vec4 color, DebugStyle style, int segments) {
    Vec3 u, v;
    basis(normal, u, v);
    segments = segments < 3 ? 3 : segments;
    Vec3 prev = center + u * radius;
    for (int i = 1; i <= segments; ++i) {
        const float a = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 p = center + (u * std::cos(a) + v * std::sin(a)) * radius;
        line(prev, p, color, style);
        prev = p;
    }
}

void DebugDraw::sphere(Vec3 center, float radius, Vec4 color, DebugStyle style, int segments) {
    circle(center, {1, 0, 0}, radius, color, style, segments);
    circle(center, {0, 1, 0}, radius, color, style, segments);
    circle(center, {0, 0, 1}, radius, color, style, segments);
}

void DebugDraw::cylinder(Vec3 center, Quat q, float radius, float half_height, Vec4 color, DebugStyle style) {
    const Vec3 up = rotate_vec(q, {0, 1, 0});
    const Vec3 top = center + up * half_height, bottom = center - up * half_height;
    circle(top, up, radius, color, style);
    circle(bottom, up, radius, color, style);
    for (Vec3 side : {Vec3{1, 0, 0}, Vec3{-1, 0, 0}, Vec3{0, 0, 1}, Vec3{0, 0, -1}}) {
        const Vec3 o = rotate_vec(q, side) * radius;
        line(top + o, bottom + o, color, style);
    }
}

void DebugDraw::capsule(Vec3 center, Quat q, float radius, float half_height, Vec4 color, DebugStyle style) {
    cylinder(center, q, radius, half_height, color, style);
    const Vec3 up = rotate_vec(q, {0, 1, 0});
    for (float sign : {1.0f, -1.0f}) { // half circles over the caps, in two planes
        const Vec3 base = center + up * (half_height * sign);
        for (Vec3 axis : {Vec3{1, 0, 0}, Vec3{0, 0, 1}}) {
            const Vec3 side = rotate_vec(q, axis);
            Vec3 prev = base + side * radius;
            for (int i = 1; i <= 12; ++i) {
                const float a = std::numbers::pi_v<float> * static_cast<float>(i) / 12.0f;
                const Vec3 p = base + (side * std::cos(a) + up * (std::sin(a) * sign)) * radius;
                line(prev, p, color, style);
                prev = p;
            }
        }
    }
}

void DebugDraw::arrow(Vec3 from, Vec3 to, Vec4 color, DebugStyle style) {
    line(from, to, color, style);
    const Vec3 d = to - from;
    const float len = length(d);
    if (len < 1e-6f) return;
    Vec3 u, v;
    basis(d, u, v);
    const float head = std::min(len * 0.25f, 0.3f);
    const Vec3 back = to - d * (head / len);
    for (Vec3 s : {u, u * -1.0f, v, v * -1.0f}) line(to, back + s * (head * 0.4f), color, style);
}

void DebugDraw::axes(const Mat4& m, float size, DebugStyle style) {
    const Vec3 o{m[3].x, m[3].y, m[3].z};
    const Vec3 x{m[0].x, m[0].y, m[0].z}, y{m[1].x, m[1].y, m[1].z}, z{m[2].x, m[2].y, m[2].z};
    arrow(o, o + normalize(x) * size, {1, 0.15f, 0.15f, 1}, style);
    arrow(o, o + normalize(y) * size, {0.2f, 1, 0.2f, 1}, style);
    arrow(o, o + normalize(z) * size, {0.25f, 0.4f, 1, 1}, style);
}

void DebugDraw::cross(Vec3 p, float s, Vec4 color, DebugStyle style) {
    line(p - Vec3{s, 0, 0}, p + Vec3{s, 0, 0}, color, style);
    line(p - Vec3{0, s, 0}, p + Vec3{0, s, 0}, color, style);
    line(p - Vec3{0, 0, s}, p + Vec3{0, 0, s}, color, style);
}

void DebugDraw::grid(Vec3 c, float size, int cells, Vec4 color, DebugStyle style) {
    cells = cells < 1 ? 1 : cells;
    const float h = size * 0.5f, step = size / static_cast<float>(cells);
    for (int i = 0; i <= cells; ++i) {
        const float t = -h + step * static_cast<float>(i);
        line(c + Vec3{t, 0, -h}, c + Vec3{t, 0, h}, color, style);
        line(c + Vec3{-h, 0, t}, c + Vec3{h, 0, t}, color, style);
    }
}

void DebugDraw::advance(float seconds) {
    std::size_t keep = 0;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const float left = remaining_[i] - seconds;
        if (remaining_[i] <= 0.0f || left <= 0.0f) continue;
        lines_[keep] = lines_[i];
        remaining_[keep] = left;
        ++keep;
    }
    lines_.resize(keep);
    remaining_.resize(keep);
}

void DebugDraw::clear() noexcept {
    lines_.clear();
    remaining_.clear();
}

} // namespace astral
