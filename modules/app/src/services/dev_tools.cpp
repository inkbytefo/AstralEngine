#include "dev_tools.hpp"

#include "../debug_ui.hpp"
#include "../material_library.hpp"
#include "world_bridge.hpp"

#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>
#include <astral/render/text.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <algorithm>

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
        if (!md.grids.empty()) debug.circle(p, {0, 1, 0}, static_cast<float>(md.grids[0].load_range), {0.2f, 1.0f, 0.4f, 1});
    }
}

void draw_shape_overlay(DebugDraw& debug, const ShapeDesc& s, Vec3 pos, Quat rot, Vec4 color) {
    switch (s.type) {
    case ShapeType::Box: debug.box(pos, s.half_extents, rot, color); break;
    case ShapeType::Sphere: debug.sphere(pos, s.radius, color); break;
    case ShapeType::Capsule: debug.capsule(pos, rot, s.radius, s.half_height, color); break;
    case ShapeType::Cylinder: debug.cylinder(pos, rot, s.radius, s.half_height, color); break;
    case ShapeType::ConvexHull: { // its bounding box
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
    case ShapeType::HeightField: { // every 4th row and column
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
        const float half = cc.desc.height * 0.5f; // feet at the entity position
        debug.capsule({w.matrix[3].x, w.matrix[3].y + half, w.matrix[3].z}, Quat{}, cc.desc.radius,
                      std::max(0.0f, half - cc.desc.radius), cc.on_ground ? Vec4{1, 0.85f, 0.2f, 1} : Vec4{1, 0.4f, 0.2f, 1});
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

DevTools::DevTools(World& world, Window* window, Renderer* renderer, bool enable_debug_ui)
    : world_(world), window_(window) {
    if (renderer && enable_debug_ui) debug_ui_ = std::make_unique<DebugUi>(*renderer);
    font_ = Font::create(default_font_ttf());
    if (font_ && renderer) font_->upload(*renderer);
}

DevTools::~DevTools() {
    debug_ui_.reset(); // owns renderer textures
    font_.reset();
}

InputCapture DevTools::route_debug_input(const Input& raw_input, bool& show_debug_ui) {
    if (debug_ui_ && raw_input.pressed(Key::F1)) {
        show_debug_ui = !show_debug_ui;
        if (window_ && show_debug_ui && window_->mouse_captured()) { // the UI needs a cursor
            window_->set_mouse_captured(false);
            restore_mouse_capture_ = true;
        } else if (window_ && !show_debug_ui && restore_mouse_capture_) {
            window_->set_mouse_captured(true);
            restore_mouse_capture_ = false;
        }
    }
    if (debug_ui_ && show_debug_ui) { // what the UI used last frame is not the game's
        return {.keyboard = debug_ui_->wants_keyboard(), .mouse = debug_ui_->wants_mouse()};
    }
    return {};
}

void DevTools::draw_overlays(const Context::DebugShow& show, DebugDraw& debug, const WorldCellsOverlay* world_cells) const {
    if (show.world_cells && world_cells) draw_world_cells_overlay(debug, *world_cells);
    if (show.physics) draw_physics_overlay(world_, debug);
    if (show.fields) draw_fields_overlay(world_, debug);
    if (show.lights) draw_lights_overlay(world_, debug);
}

void DevTools::build_ui(Game& game, Context& ctx, const Input& raw_input, float delta, gpu::Extent2D extent,
                        MaterialLibrary* materials) {
    ui_layer_ = {};
    const bool visible = debug_ui_ && ctx.show_debug_ui;
    if (window_ && text_input_ != (visible && debug_ui_->wants_text())) {
        text_input_ = !text_input_;
        window_->set_text_input(text_input_);
    }
    if (!visible) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "debug ui");
    debug_ui_->begin(raw_input, delta, extent, window_ ? window_->pixel_density() : 1.0f);
    debug_ui_->draw_panels(ctx, materials);
    game.on_debug_ui(ctx);
    debug_ui_->end();
    ui_layer_ = debug_ui_->layer();
}

std::span<const UiLayer> DevTools::overlay_layers(const UiCanvas& hud) {
    std::size_t n = 0;
    if (!hud.empty()) layers_[n++] = hud.layer();
    if (!ui_layer_.commands.empty()) layers_[n++] = ui_layer_;
    return std::span<const UiLayer>(layers_.data(), n);
}

} // namespace astral
