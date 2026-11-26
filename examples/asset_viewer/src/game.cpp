#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/orbit_camera/orbit_camera.hpp"
#include "studio.hpp"

#include <astral/app/feature.hpp>
#include <astral/assets/pack.hpp>
#include <astral/core/json.hpp>
#include <astral/gpu/device.hpp>
#include <astral/platform/paths.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>

namespace asset_viewer {

using namespace astral;

namespace {

std::optional<std::string> read_fallback_text(const std::filesystem::path& dir, const Pack* pack,
                                              std::string_view rel_path) {
    if (pack) {
        if (const auto bytes = pack->read(rel_path)) {
            return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        }
    }
    if (!dir.empty()) {
        std::ifstream in(dir / rel_path, std::ios::binary | std::ios::ate);
        if (in) {
            const auto sz = in.tellg();
            if (sz >= 0) {
                std::string text(static_cast<std::size_t>(sz), '\0');
                in.seekg(0);
                if (text.empty() || in.read(text.data(), sz)) return text;
            }
        }
    }
    return std::nullopt;
}

std::unique_ptr<Pack> open_fallback_pack(std::string_view pack_name) {
    if (pack_name.empty()) return nullptr;
    std::filesystem::path p(pack_name);
    if (p.is_relative()) p = executable_directory() / p;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec)) return nullptr;
    return Pack::open(p);
}

bool install_features_from_fallback(Context& ctx, std::string_view fallback_assets, std::string_view fallback_pack) {
    const std::filesystem::path dir(fallback_assets);
    const std::unique_ptr<Pack> pack = open_fallback_pack(fallback_pack);

    FeatureManifest fly_m{.name = "fly_camera", .components = {"fly::FlyCamera"}};
    FeatureManifest orbit_m{.name = "orbit_camera", .components = {"orbit::OrbitCamera"}};
    FeatureSet set;
    set.add(std::move(fly_m), fly::install_fly_camera);
    set.add(std::move(orbit_m), orbit::install_orbit_camera);
    if (!set.install(ctx)) return false;

    Json merged_actions = Json::object();
    for (const char* path : {"features/fly_camera/fly_camera.actions", "features/orbit_camera/orbit_camera.actions"}) {
        const auto text = read_fallback_text(dir, pack.get(), path);
        if (!text) continue;
        if (const auto doc = Json::parse(*text)) {
            if (const Json* acts = doc->find("actions"); acts && acts->is_object()) {
                for (const auto& [name, spec] : acts->as_object()) merged_actions[name] = spec;
            }
        }
    }
    Json root = Json::object();
    root["actions"] = std::move(merged_actions);
    std::string err;
    if (!ctx.actions.load(root, &err)) {
        ctx.log.error("asset_viewer fallback actions: {}", err);
        return false;
    }

    for (const char* path : {"features/fly_camera/fly_camera.luau", "features/orbit_camera/orbit_camera.luau"}) {
        const auto text = read_fallback_text(dir, pack.get(), path);
        if (!text) continue;
        const auto problems = ctx.scripts->load(path, *text);
        if (!problems.empty()) {
            ctx.log.error("asset_viewer fallback script {}: {}", path, problems.front());
            return false;
        }
    }
    return true;
}

void sync_models_if_headless(Context& ctx) {
    if (ctx.window || !ctx.assets) return;
    ctx.world.each<const ModelRef>([&](const ModelRef& ref) {
        if (!ref.path.empty()) ctx.assets->load_model(ref.path);
    });
    ctx.assets->wait_all();
}

void compute_scene_home_pose(const Transform& t, Vec3& out_eye, Vec3& out_target) {
    out_eye = t.position;
    const Vec3 forward = normalize(t.rotation * Vec3{0.0f, 0.0f, -1.0f});
    const float along = dot(-out_eye, forward);
    const float dist = std::clamp(along > 1.0f ? along : 6.0f, 1.0f, 30.0f);
    out_target = out_eye + forward * dist;
}

void apply_orbit_or_fly(World& w, Entity cam, CameraMode mode, Vec3 eye, Vec3 target) {
    if (!w.alive(cam)) return;
    const orbit::OrbitCamera orb = orbit::from_look_at(eye, target);
    const Quat rot = orbit::camera_rotation(orb.yaw, orb.pitch);
    if (Transform* t = w.get<Transform>(cam)) {
        t->position = eye;
        t->rotation = rot;
    }
    if (mode == CameraMode::Orbit) {
        w.remove<fly::FlyCamera>(cam);
        w.add(cam, orb);
    } else {
        w.remove<orbit::OrbitCamera>(cam);
        w.add(cam, fly::FlyCamera{.speed = 4.0f, .fast_speed = 12.0f, .yaw = orb.yaw, .pitch = orb.pitch});
    }
}

} // namespace

bool install_features(Context& ctx, std::string_view fallback_assets, std::string_view fallback_pack) {
    if (ctx.assets && ctx.assets->read("features/fly_camera/feature.json")) {
        FeatureSet game;
        game.add("fly_camera", fly::install_fly_camera);
        game.add("orbit_camera", orbit::install_orbit_camera);
        return game.install(ctx);
    }
    return install_features_from_fallback(ctx, fallback_assets, fallback_pack);
}

ViewerGame::ViewerGame(ViewerOptions options)
    : options_(std::move(options)),
      station_(options_.station),
      camera_mode_(options_.camera_mode),
      debug_(options_.debug),
      active_file_(options_.file),
      settings_(options_.station == StationId::Model ? studio_settings() : scene_settings()) {
    settings_.ray_tracing = !options_.raster;
}

void ViewerGame::on_start(Context& ctx) {
    if (!install_features(ctx, options_.fallback_assets, options_.fallback_pack)) {
        ctx.quit();
        return;
    }
    if (ctx.device && !ctx.device->supports(gpu::Feature::RayTracing)) {
        options_.raster = true;
        settings_.ray_tracing = false;
    }
    ctx.show_debug_ui = options_.ui;
    apply_debug_flags(ctx);
    const std::string file = active_file_.empty() ? std::string(default_file_for(station_)) : active_file_;
    open_view(ctx, station_, file);
}

void ViewerGame::clear_owned(Context& ctx) {
    for (Entity e : owned_) {
        if (ctx.world.alive(e)) ctx.world.destroy(e);
    }
    owned_.clear();
    camera_ = {};
    doc_id_ = {};
    doc_version_ = 0;
}

void ViewerGame::open_view(Context& ctx, StationId id, std::string file) {
    clear_owned(ctx);
    station_ = id;
    active_file_ = std::move(file);
    tracker_.reset(options_.asset_root);

    if (station_ == StationId::Model) {
        load_model_view(ctx, active_file_);
    } else {
        load_scene_view(ctx, active_file_);
    }

    RenderSettings next = station_ == StationId::Model ? studio_settings() : scene_settings();
    next.ray_tracing = !options_.raster;
    settings_ = next;
    if (ctx.renderer) {
        ctx.renderer->set_settings(settings_);
        ctx.renderer->reset_history();
    }
    sync_models_if_headless(ctx);
    stats_ = tracker_.collect(ctx, scene_reloads_);
}

void ViewerGame::load_scene_view(Context& ctx, const std::string& file) {
    ctx.sun = {};
    ctx.environment = {};
    const LoadResult r = ctx.scenes().load_scene(file);
    owned_ = r.entities;
    doc_id_ = ctx.assets->load_document(file);
    const Document* doc = ctx.assets->document(doc_id_);
    doc_version_ = doc ? doc->version : 0;

    if (options_.raster && ctx.world.count<RectLight>() > 0 && ctx.world.count<PointLight>() == 0 &&
        ctx.sun.intensity == 0.0f) {
        Vec3 pos{0.0f, 4.5f, 0.0f};
        Vec3 col{1.0f, 0.95f, 0.88f};
        ctx.world.each<const Transform, const RectLight>([&](const Transform& t, const RectLight& rl) {
            pos = t.position;
            col = rl.color;
        });
        owned_.push_back(ctx.world.create(Name{"raster stand-in light"}, Transform{.position = pos},
                                          PointLight{.position = pos, .color = col, .intensity = 22.0f, .radius = 18.0f}));
    }

    Entity cam{};
    ctx.world.each<const Camera>([&](Entity e, const Camera& c) {
        if (!cam.valid() && c.active) cam = e;
    });
    if (!cam.valid()) {
        const Studio fallback;
        const Quat rot = rotation_of(inverse(look_at(fallback.eye, fallback.target, {0.0f, 1.0f, 0.0f})));
        cam = ctx.world.create(Name{"viewer camera"}, Transform{.position = fallback.eye, .rotation = rot},
                               Camera{.fov_y = radians(45.0f), .near_plane = 0.05f});
        owned_.push_back(cam);
    }
    camera_ = cam;
    const Transform* t = ctx.world.get<const Transform>(cam);
    compute_scene_home_pose(t ? *t : Transform{}, home_eye_, home_target_);
    apply_orbit_or_fly(ctx.world, camera_, camera_mode_, home_eye_, home_target_);
    ctx.log.info("asset_viewer scene {}: {} entities, {} problems", file, r.entities.size(), r.errors.size());
}

void ViewerGame::load_model_view(Context& ctx, const std::string& file) {
    const Studio studio;
    camera_ = build_model_studio(ctx, file, options_.raster, floor_material_, owned_);
    home_eye_ = studio.eye;
    home_target_ = studio.target;
    apply_orbit_or_fly(ctx.world, camera_, camera_mode_, home_eye_, home_target_);
    ctx.log.info("asset_viewer model {} — edit and re-export to hot-reload", file);
}

void ViewerGame::set_camera_mode(Context& ctx, CameraMode mode) {
    if (mode == camera_mode_ || !ctx.world.alive(camera_)) return;
    const Transform* t = ctx.world.get<const Transform>(camera_);
    const Vec3 eye = t ? t->position : home_eye_;
    float yaw = 0.0f, pitch = 0.0f;
    float distance = std::max(length(home_target_ - home_eye_), 1.0f);
    if (const auto* orb = ctx.world.get<const orbit::OrbitCamera>(camera_)) {
        yaw = orb->yaw;
        pitch = orb->pitch;
        distance = orb->distance;
    } else if (const auto* fl = ctx.world.get<const fly::FlyCamera>(camera_)) {
        yaw = fl->yaw;
        pitch = fl->pitch;
    }
    camera_mode_ = mode;
    if (mode == CameraMode::Fly) {
        ctx.world.remove<orbit::OrbitCamera>(camera_);
        ctx.world.add(camera_, fly::FlyCamera{.speed = 4.0f, .fast_speed = 12.0f, .yaw = yaw, .pitch = pitch});
    } else {
        ctx.world.remove<fly::FlyCamera>(camera_);
        const Vec3 forward = orbit::camera_rotation(yaw, pitch) * Vec3{0.0f, 0.0f, -1.0f};
        orbit::OrbitCamera orb{};
        orb.target = eye + forward * distance;
        orb.distance = std::clamp(distance, orb.min_distance, orb.max_distance);
        orb.yaw = yaw;
        orb.pitch = pitch;
        ctx.world.add(camera_, orb);
    }
}

void ViewerGame::reset_camera(Context& ctx) {
    apply_orbit_or_fly(ctx.world, camera_, camera_mode_, home_eye_, home_target_);
    if (ctx.renderer) ctx.renderer->reset_history();
}

void ViewerGame::apply_debug_flags(Context& ctx) {
    ctx.debug_show.lights = debug_;
    ctx.debug_show.physics = debug_;
}

void ViewerGame::check_scene_hot_reload(Context& ctx) {
    if (station_ != StationId::Scene || !doc_id_.valid() || !ctx.assets) return;
    const Document* doc = ctx.assets->document(doc_id_);
    if (!doc || doc->version == doc_version_) return;
    ++scene_reloads_;
    open_view(ctx, StationId::Scene, active_file_);
}

void ViewerGame::on_update(Context& ctx) {
    check_scene_hot_reload(ctx);

    if (!options_.external) {
        if (ctx.input.pressed(Key::Num1) && station_ != StationId::Scene) {
            open_view(ctx, StationId::Scene, std::string(default_file_for(StationId::Scene)));
        } else if (ctx.input.pressed(Key::Num2) && station_ != StationId::Model) {
            open_view(ctx, StationId::Model, std::string(default_file_for(StationId::Model)));
        }
    }
    if (ctx.input.pressed(Key::C)) {
        set_camera_mode(ctx, camera_mode_ == CameraMode::Orbit ? CameraMode::Fly : CameraMode::Orbit);
    }
    if (ctx.input.pressed(Key::R)) reset_camera(ctx);
    if (ctx.input.pressed(Key::G)) {
        debug_ = !debug_;
        apply_debug_flags(ctx);
    }

    if (debug_) {
        ctx.debug.grid({0.0f, 0.01f, 1.0f}, 6.0f, 12, {1.0f, 1.0f, 1.0f, 0.25f});
        ctx.world.each<const Name, const WorldTransform>(
            [&](const Name&, const WorldTransform& w) { ctx.debug.axes(w.matrix, 0.3f); });
    }

    stats_ = tracker_.collect(ctx, scene_reloads_);
    draw_viewer_hud(ctx, stats_, station_, active_file_, camera_mode_, debug_, options_.external, settings_);
}

} // namespace asset_viewer
