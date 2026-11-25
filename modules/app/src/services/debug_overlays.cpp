#include "debug_overlays.hpp"
#include "world_bridge.hpp"

#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>
#include <optional>

namespace astral {

namespace {

void draw_world_cells_overlay(DebugDraw& debug, const WorldCellsOverlay& cells) {
    if (!cells.map) return;
    const MapDesc& md = *cells.map;
    const WorldPosition o = cells.origin;
    for (const CellKey& key : cells.attached_cells) {
        if (key.grid >= md.grids.size()) continue;
        const auto size = static_cast<float>(md.grids[key.grid].cell_size);
        const Vec3 a{static_cast<float>(cell_min_x(md, key) - o.x), 0.05f, static_cast<float>(cell_min_z(md, key) - o.z)};
        const Vec4 color = key.grid == 0 ? Vec4{1.0f, 0.8f, 0.2f, 1} : Vec4{0.4f, 0.8f, 1.0f, 1};
        debug.line(a, {a.x + size, a.y, a.z}, color);
        debug.line(a, {a.x, a.y, a.z + size}, color);
        debug.line({a.x + size, a.y, a.z}, {a.x + size, a.y, a.z + size}, color);
        debug.line({a.x, a.y, a.z + size}, {a.x + size, a.y, a.z + size}, color);
    }
    for (const Vec3& p : cells.sources) {
        if (!md.grids.empty()) {
            debug.circle(p, {0, 1, 0}, static_cast<float>(md.grids[0].load_range), {0.2f, 1.0f, 0.4f, 1});
        }
    }
}

void draw_shape_overlay(DebugDraw& debug, const ShapeDesc& s, Vec3 pos, Quat rot, Vec4 color) {
    switch (s.type) {
    case ShapeType::Box: debug.box(pos, s.half_extents, rot, color); break;
    case ShapeType::Sphere: debug.sphere(pos, s.radius, color); break;
    case ShapeType::Capsule: debug.capsule(pos, rot, s.radius, s.half_height, color); break;
    case ShapeType::Cylinder: debug.cylinder(pos, rot, s.radius, s.half_height, color); break;
    case ShapeType::ConvexHull: {
        Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
        for (const Vec3& v : s.points) {
            lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
            hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
        }
        debug.box(pos + rot * ((lo + hi) * 0.5f), (hi - lo) * 0.5f, rot, color);
        break;
    }
    case ShapeType::Mesh: {
        const std::size_t triangles = std::min<std::size_t>(s.indices.size() / 3, 4000);
        for (std::size_t i = 0; i < triangles; ++i) {
            const Vec3 a2 = pos + rot * s.vertices[s.indices[i * 3]];
            const Vec3 b2 = pos + rot * s.vertices[s.indices[i * 3 + 1]];
            const Vec3 c2 = pos + rot * s.vertices[s.indices[i * 3 + 2]];
            debug.line(a2, b2, color);
            debug.line(b2, c2, color);
            debug.line(c2, a2, color);
        }
        break;
    }
    case ShapeType::HeightField: {
        const float half = 0.5f * static_cast<float>(s.samples - 1) * s.cell_size;
        const auto at = [&](std::uint32_t x, std::uint32_t z) {
            return pos + rot * Vec3{static_cast<float>(x) * s.cell_size - half, s.heights[z * s.samples + x],
                                    static_cast<float>(z) * s.cell_size - half};
        };
        for (std::uint32_t z = 0; z < s.samples; z += 4) {
            for (std::uint32_t x = 0; x + 1 < s.samples; ++x) debug.line(at(x, z), at(x + 1, z), color);
        }
        for (std::uint32_t x = 0; x < s.samples; x += 4) {
            for (std::uint32_t z = 0; z + 1 < s.samples; ++z) debug.line(at(x, z), at(x, z + 1), color);
        }
        break;
    }
    case ShapeType::Compound:
        for (const ShapeChild& child : s.children) {
            draw_shape_overlay(debug, child.shape, pos + rot * child.position, rot * child.rotation, color);
        }
        break;
    }
}

void draw_physics_overlay(World& world, DebugDraw& debug) {
    world.each<const RigidBody, const WorldTransform>([&](const RigidBody& rb, const WorldTransform& w) {
        const Vec3 p{w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
        const Quat q = rotation_of(w.matrix);
        const Vec4 color = rb.desc.motion == MotionType::Dynamic     ? Vec4{0.3f, 1.0f, 0.4f, 1}
                           : rb.desc.motion == MotionType::Kinematic ? Vec4{0.3f, 0.6f, 1.0f, 1}
                                                                     : Vec4{0.6f, 0.6f, 0.6f, 1};
        draw_shape_overlay(debug, rb.desc.shape, p, q, color);
    });
    world.each<const CharacterController, const WorldTransform>([&](const CharacterController& cc, const WorldTransform& w) {
        const float half = cc.desc.height * 0.5f;
        debug.capsule({w.matrix[3].x, w.matrix[3].y + half, w.matrix[3].z}, Quat{}, cc.desc.radius,
                      std::max(0.0f, half - cc.desc.radius), cc.on_ground ? Vec4{1, 0.85f, 0.2f, 1} : Vec4{1, 0.4f, 0.2f, 1});
    });
}

void draw_joints_overlay(World& world, DebugDraw& debug) {
    auto entity_pos = [&](Entity e) -> std::optional<Vec3> {
        if (!e.valid() || !world.alive(e)) return std::nullopt;
        if (const WorldTransform* w = world.get<const WorldTransform>(e)) {
            return Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        }
        if (const Transform* t = world.get<const Transform>(e)) return t->position;
        return std::nullopt;
    };
    world.each<const Joint>([&](Entity e, const Joint& j) {
        const JointMotor* m = world.get<const JointMotor>(e);
        const bool powered = m && m->mode != JointMotorMode::Off && !j.broken;
        const Vec4 color = j.broken   ? Vec4{1.0f, 0.2f, 0.2f, 1.0f}
                           : powered  ? Vec4{0.2f, 0.9f, 1.0f, 1.0f}
                                      : Vec4{1.0f, 0.8f, 0.2f, 1.0f};
        debug.cross(j.anchor, j.broken ? 0.25f : 0.15f, color, {.on_top = true});
        if (const auto pa = entity_pos(j.a)) debug.line(j.anchor, *pa, color, {.on_top = true});
        if (const auto pb = entity_pos(j.b)) {
            const Vec3 target = j.type == JointType::Distance ? j.anchor_b : j.anchor;
            debug.line(target, *pb, color, {.on_top = true});
            if (j.type == JointType::Distance) debug.line(j.anchor, j.anchor_b, color, {.on_top = true});
        }
        if (length(j.axis) > 1e-4f) {
            const Vec3 ax = normalize(j.axis);
            debug.arrow(j.anchor, j.anchor + ax * 0.55f, color, {.on_top = true});
            if (j.type == JointType::SwingTwist && j.swing_limits.x > 0.01f) {
                const float r = std::sin(std::min(j.swing_limits.x, 1.5f)) * 0.5f;
                const float d = std::cos(std::min(j.swing_limits.x, 1.5f)) * 0.5f;
                debug.circle(j.anchor + ax * d, ax, r, {color.x, color.y, color.z, 0.7f});
            }
        }
    });
}

void draw_fields_overlay(World& world, DebugDraw& debug) {
    world.each<const WindZone, const WorldTransform>([&](const WindZone& z, const WorldTransform& w) {
        const Vec3 p{w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
        if (!z.global) debug.box(p, z.half_extents, Quat{}, {0.5f, 0.9f, 1.0f, 1});
        const float speed = length(z.velocity);
        if (speed > 0.0f) {
            debug.arrow(p, p + normalize(z.velocity) * std::min(1.0f + speed * 0.3f, 6.0f), {0.5f, 0.9f, 1.0f, 1},
                        {.on_top = true});
        }
    });
    world.each<const WaterBody, const WorldTransform>([&](const WaterBody& b, const WorldTransform& w) {
        debug.box({w.matrix[3].x, w.matrix[3].y, w.matrix[3].z}, b.half_extents, Quat{}, {0.2f, 0.4f, 1.0f, 1});
    });
}

void draw_lights_overlay(World& world, DebugDraw& debug) {
    world.each<const PointLight>([&](Entity e, const PointLight& l) {
        Vec3 p = l.position;
        if (const WorldTransform* w = world.get<WorldTransform>(e)) p = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        const Vec4 c{l.color.x, l.color.y, l.color.z, 1};
        debug.cross(p, 0.15f, c, {.on_top = true});
        debug.sphere(p, l.radius, {c.x, c.y, c.z, 0.35f});
    });
    world.each<const RectLight>([&](Entity e, const RectLight& l) {
        Vec3 p = l.position, r = l.right, u = l.up;
        if (const WorldTransform* w = world.get<WorldTransform>(e)) {
            p = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
            const Vec4 rr = w->matrix * Vec4{r.x, r.y, r.z, 0}, uu = w->matrix * Vec4{u.x, u.y, u.z, 0};
            r = {rr.x, rr.y, rr.z};
            u = {uu.x, uu.y, uu.z};
        }
        const Vec4 c{l.color.x, l.color.y, l.color.z, 1};
        const Vec3 a = p - r - u, b = p + r - u, cc = p + r + u, d = p - r + u;
        debug.line(a, b, c, {.on_top = true});
        debug.line(b, cc, c, {.on_top = true});
        debug.line(cc, d, c, {.on_top = true});
        debug.line(d, a, c, {.on_top = true});
        debug.arrow(p, p + normalize(cross(r, u)) * 0.5f, c, {.on_top = true});
    });
}

} // namespace

void draw_debug_overlays(World& world, const Context::DebugShow& show, DebugDraw& debug,
                         const WorldCellsOverlay* world_cells) {
    if (show.world_cells && world_cells) draw_world_cells_overlay(debug, *world_cells);
    if (show.physics) draw_physics_overlay(world, debug);
    if (show.physics || show.physics_joints) draw_joints_overlay(world, debug);
    if (show.fields) draw_fields_overlay(world, debug);
    if (show.lights) draw_lights_overlay(world, debug);
}

} // namespace astral
