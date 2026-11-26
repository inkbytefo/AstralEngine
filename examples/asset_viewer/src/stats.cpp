#include "stats.hpp"

#include <astral/assets/cook.hpp>
#include <astral/assets/model.hpp>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>

namespace asset_viewer {

using namespace astral;

namespace {

std::optional<ModelData> load_model_data_for_stats(const AssetManager& assets, const std::filesystem::path& asset_root,
                                                   std::string_view rel_path) {
    if (const auto bytes = assets.read(rel_path)) {
        if (is_cooked_model(*bytes)) return read_cooked_model(*bytes);
    }
    if (assets.from_pack()) return std::nullopt;
    return load_gltf(asset_root / rel_path);
}

void accumulate_bounds(const ModelData& data, Vec3& lo, Vec3& hi, bool& has_vertex) {
    struct Item {
        std::uint32_t node;
        Mat4 parent;
    };
    std::vector<Item> stack;
    for (std::uint32_t r : data.roots) stack.push_back({r, Mat4::identity()});
    while (!stack.empty()) {
        const Item item = stack.back();
        stack.pop_back();
        if (item.node >= data.nodes.size()) continue;
        const ModelNode& n = data.nodes[item.node];
        const Mat4 world = item.parent * compose(n.translation, n.rotation, n.scale);
        for (std::uint32_t p_idx : n.primitives) {
            if (p_idx >= data.primitives.size()) continue;
            for (const Vertex& v : data.primitives[p_idx].mesh.vertices) {
                const Vec4 wp = world * Vec4{v.position.x, v.position.y, v.position.z, 1.0f};
                if (!has_vertex) {
                    lo = hi = {wp.x, wp.y, wp.z};
                    has_vertex = true;
                } else {
                    lo = {std::min(lo.x, wp.x), std::min(lo.y, wp.y), std::min(lo.z, wp.z)};
                    hi = {std::max(hi.x, wp.x), std::max(hi.y, wp.y), std::max(hi.z, wp.z)};
                }
            }
        }
        for (std::uint32_t child : n.children) stack.push_back({child, world});
    }
}

void inspect_single_model(const AssetManager& assets, const std::filesystem::path& asset_root,
                          std::string_view rel_path, ModelStats& out, Vec3& lo, Vec3& hi, bool& has_vertex) {
    const AssetId id = AssetId::from_path(rel_path);
    const AssetState st = assets.state(id);
    if (st == AssetState::Failed) {
        ++out.failed_models;
        return;
    }
    const Model* runtime = assets.model(id);
    if (!runtime) return;
    ++out.loaded_models;
    out.nodes += runtime->nodes.size();
    for (const Model::Node& n : runtime->nodes) out.pieces += n.pieces.size();
    out.materials += runtime->material_slots.size();

    if (const auto data = load_model_data_for_stats(assets, asset_root, rel_path)) {
        for (const ModelPrimitive& prim : data->primitives) {
            out.vertices += prim.mesh.vertices.size();
            out.triangles += prim.mesh.indices.size() / 3;
        }
        out.images += data->images.size();
        out.warnings += data->warnings.size();
        accumulate_bounds(*data, lo, hi, has_vertex);
    }
}

std::string format_bounds(Vec3 b) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f x %.2f x %.2f m", b.x, b.y, b.z);
    return buf;
}

std::string format_renderer_line(const Renderer& r) {
    const RenderStats& rs = r.stats();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Çizim: %u · Örnek: %u · GPU: %.2f ms", rs.draw_calls, rs.instances,
                  r.gpu_frame_ms());
    return buf;
}

std::vector<std::string> build_hud_lines(Context& ctx, const ViewerStats& s, CameraMode camera, bool debug_overlays,
                                         const RenderSettings& render) {
    std::vector<std::string> lines;
    std::string source = s.from_pack ? "Kaynak: paket (.apak)" : "Kaynak: gevşek dosya (hot-reload)";
    const std::uint32_t reloads = s.asset_reloads + s.scene_reloads;
    if (reloads > 0) source += " · yenileme: " + std::to_string(reloads);
    lines.push_back(std::move(source));

    const std::string cam_name = camera == CameraMode::Orbit ? "Yörünge" : "Uçuş";
    const std::string mode_name = render.ray_tracing ? "hibrit ışın izleme" : "raster";
    lines.push_back("Kamera: " + cam_name + " · Mod: " + mode_name + (debug_overlays ? " · Debug açık" : ""));

    lines.push_back("Varlık: " + std::to_string(s.entities) + " · Mesh: " + std::to_string(s.meshes) +
                    " · Prefab: " + std::to_string(s.prefab_instances) + " · SDF: " + std::to_string(s.sdf_shapes));

    const std::size_t lights = s.rect_lights + s.point_lights + (s.has_sun ? 1u : 0u);
    lines.push_back("Işık: " + std::to_string(lights) + " (alan " + std::to_string(s.rect_lights) + ", nokta " +
                    std::to_string(s.point_lights) + (s.has_sun ? ", güneş)" : ")"));

    if (s.model_refs > 0) {
        const ModelStats& m = s.model;
        lines.push_back("Model: " + std::to_string(m.nodes) + " düğüm · " + std::to_string(m.triangles) + " üçgen · " +
                        std::to_string(m.vertices) + " köşe");
        lines.push_back("Materyal: " + std::to_string(m.materials) + " · Doku: " + std::to_string(m.images) +
                        " · Boyut: " + format_bounds(m.bounds_size));
        if (m.warnings > 0 || m.failed_models > 0) {
            lines.push_back("Uyarı: " + std::to_string(m.warnings) + " · Hata: " + std::to_string(m.failed_models));
        }
    }
    if (ctx.renderer) lines.push_back(format_renderer_line(*ctx.renderer));
    return lines;
}

} // namespace

void StatsTracker::reset(std::filesystem::path asset_root) {
    asset_root_ = std::move(asset_root);
    cached_paths_.clear();
    cached_reloads_ = 0;
    cache_ready_ = false;
    cached_model_ = {};
}

ViewerStats StatsTracker::collect(Context& ctx, std::uint32_t scene_reloads) {
    World& w = ctx.world;
    ViewerStats s{};
    s.entities = w.count();
    s.meshes = w.count<MeshRenderer>();
    s.primitives = w.count<PrimitiveMesh>();
    s.model_refs = w.count<ModelRef>();
    s.prefab_instances = w.count<PrefabInstance>();
    s.prefab_parts = w.count<PrefabPart>();
    s.sdf_shapes = w.count<Sdf>();
    s.rect_lights = w.count<RectLight>();
    s.point_lights = w.count<PointLight>();
    s.cameras = w.count<Camera>();
    s.scene_reloads = scene_reloads;
    s.has_sun = ctx.sun.intensity > 0.0f;

    if (!ctx.assets) return s;
    s.from_pack = ctx.assets->from_pack();
    s.asset_reloads = ctx.assets->reloads();

    std::vector<std::string> paths;
    w.each<const ModelRef>([&](const ModelRef& ref) {
        if (!ref.path.empty()) paths.push_back(ref.path);
    });
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    bool any_loading = false;
    std::size_t ready_or_failed = 0;
    for (const std::string& p : paths) {
        const AssetState st = ctx.assets->state(AssetId::from_path(p));
        if (st == AssetState::Loading || st == AssetState::Unknown) any_loading = true;
        else ++ready_or_failed;
    }

    const bool stale = !cache_ready_ || paths != cached_paths_ || s.asset_reloads != cached_reloads_ ||
                       ready_or_failed != (cached_model_.loaded_models + cached_model_.failed_models);
    if (stale) {
        ModelStats fresh{};
        Vec3 lo{}, hi{};
        bool has_vertex = false;
        for (const std::string& p : paths) {
            inspect_single_model(*ctx.assets, asset_root_, p, fresh, lo, hi, has_vertex);
        }
        if (has_vertex) fresh.bounds_size = hi - lo;
        cached_model_ = fresh;
        cached_paths_ = std::move(paths);
        cached_reloads_ = s.asset_reloads;
        cache_ready_ = !any_loading;
    }
    s.model = cached_model_;
    return s;
}

void draw_viewer_hud(Context& ctx, const ViewerStats& stats, StationId station, std::string_view file,
                     CameraMode camera, bool debug_overlays, bool external, const RenderSettings& render) {
    if (!ctx.font) return;
    const Font& font = *ctx.font;
    const Vec2 screen = ctx.screen_size();
    const float u = screen.y / 720.0f;

    const char* title = station == StationId::Scene ? "Sahne görüntüleyici" : "Model görüntüleyici";
    const std::string subtitle = std::string(file) + " — " +
                                 (station == StationId::Scene ? "prefab, SDF, glTF ve canlı yeniden yükleme"
                                                              : "glTF/GLB stüdyo, yörünge/uçuş kamerası ve hot-reload");
    ctx.hud.text(font, title, {24 * u, 16 * u}, {.size = 36 * u, .color = {1.0f, 0.85f, 0.45f, 1.0f}, .shadow = true});
    ctx.hud.text(font, subtitle, {24 * u, 58 * u}, {.size = 16 * u, .color = {1.0f, 1.0f, 1.0f, 0.9f}, .shadow = true});

    const std::vector<std::string> lines = build_hud_lines(ctx, stats, camera, debug_overlays, render);
    const float line_h = 20.0f * u;
    const float pad = 12.0f * u;
    float max_w = 0.0f;
    for (const std::string& line : lines) max_w = std::max(max_w, font.measure(line, 15.0f * u).x);
    const float box_w = max_w + pad * 2.0f;
    const float box_h = static_cast<float>(lines.size()) * line_h + pad * 2.0f;
    const Vec2 box_min{screen.x - 20.0f * u - box_w, 16.0f * u};
    const Vec2 box_max{box_min.x + box_w, box_min.y + box_h};
    ctx.hud.rect(box_min, box_max, {0.05f, 0.06f, 0.09f, 0.72f});
    ctx.hud.frame(box_min, box_max, {0.75f, 0.85f, 1.0f, 0.25f}, std::max(1.0f, u));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        ctx.hud.text(font, lines[i], {box_min.x + pad, box_min.y + pad + static_cast<float>(i) * line_h},
                     {.size = 15.0f * u, .color = {0.86f, 0.94f, 1.0f, 0.95f}, .shadow = true});
    }

    const std::string keys =
        (external ? std::string{} : std::string("1/2 sahne/model · ")) +
        "C yörünge/uçuş · R sıfırla · G debug · F1 panel · sol sürükle/oklar yörünge · tekerlek/± yakınlaş · WASD+QE uç · Esc";
    ctx.hud.text(font, keys, {screen.x * 0.5f, screen.y - 30 * u},
                 {.size = 14.0f * u, .color = {1.0f, 1.0f, 1.0f, 0.75f}, .align = TextAlign::Center, .shadow = true});
}

} // namespace asset_viewer
