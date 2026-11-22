#include "world_bridge.hpp"

#include "../map_runtime.hpp"

#include <astral/app/components.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/render/renderer.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <fstream>
#include <string>
#include <utility>

namespace astral {

WorldBridge::WorldBridge(World& world, PrefabLibrary& prefabs, AssetManager& assets, JobSystem& jobs, Logger& log,
                         std::filesystem::path asset_root, float origin_shift_distance, float stream_budget_ms)
    : world_(world),
      prefabs_(prefabs),
      assets_(assets),
      jobs_(jobs),
      log_(log),
      asset_root_(std::move(asset_root)),
      stream_budget_ms_(stream_budget_ms),
      origin_(origin_shift_distance) {}

WorldBridge::~WorldBridge() = default;

void WorldBridge::log_problems(std::string_view what, const std::vector<std::string>& problems) const {
    for (const std::string& p : problems) log_.log(LogLevel::Error, std::string(what) + ": " + p);
}

LoadResult WorldBridge::load_scene(std::string_view path, DirectionalLight& sun, Environment& environment) {
    const AssetId id = assets_.load_document(path);
    const Document* doc = assets_.document(id);
    if (!doc) {
        LoadResult failed;
        failed.errors.push_back(std::string(assets_.error(id)));
        log_problems("load_scene", failed.errors);
        return failed;
    }
    LoadResult result = astral::load_scene(world_, prefabs_, doc->json);
    LoadContext settings;
    settings.errors = &result.errors;
    if (const Json* j = doc->json.find("sun")) {
        settings.path = "sun";
        from_json(*j, sun, settings);
    }
    if (const Json* j = doc->json.find("environment")) {
        settings.path = "environment";
        from_json(*j, environment, settings);
    }
    log_problems(std::string("load_scene ") + std::string(path), result.errors);
    return result;
}

bool WorldBridge::save_scene(std::string_view path, const DirectionalLight& sun, const Environment& environment) {
    Json doc = astral::save_scene(world_, prefabs_);
    doc["sun"] = to_json(sun, {});
    doc["environment"] = to_json(environment, {});
    const std::filesystem::path file = asset_root_ / path;
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    out << doc.dump(2) << '\n';
    if (!out) {
        log_.log(LogLevel::Error, "save_scene: cannot write " + file.string());
        return false;
    }
    return true;
}

Entity WorldBridge::spawn_prefab(std::string_view path, const Transform& transform) {
    const Entity e = world_.create(transform, PrefabInstance{std::string(path)});
    log_problems("spawn_prefab", instantiate_prefab(world_, prefabs_, e).errors);
    return e;
}

void WorldBridge::sync_prefabs() {
    prefabs_.refresh(); // templates whose documents reloaded are rebuilt on next use
    std::vector<std::string> problems;
    astral::sync_prefabs(world_, prefabs_, &problems); // instances spawned by code / prefabs that changed
    log_problems("prefab", problems);
}

bool WorldBridge::open_world(std::string_view path) {
    if (!map_) map_ = std::make_unique<MapRuntime>(world_, prefabs_, assets_, jobs_, log_);
    map_->set_origin(origin_.origin());
    return map_->open(path, asset_root_);
}

void WorldBridge::close_world() {
    if (map_) map_->close();
}

bool WorldBridge::is_open() const noexcept { return map_ && map_->is_open(); }

MapLayers* WorldBridge::layers() noexcept { return is_open() ? map_->layers() : nullptr; }

const StreamingStats* WorldBridge::streaming() const noexcept { return is_open() ? map_->stats() : nullptr; }

std::vector<Vec3> WorldBridge::streaming_sources() const {
    std::vector<Vec3> out;
    auto position_of = [&](Entity e) -> std::optional<Vec3> {
        // A top-level Transform is current even before this frame's transform update.
        if (!world_.has<Parent>(e)) {
            if (const Transform* t = world_.get<const Transform>(e)) return t->position;
        }
        if (const WorldTransform* w = world_.get<const WorldTransform>(e)) {
            return Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
        }
        return std::nullopt;
    };
    world_.each<const StreamingSource>([&](Entity e, const StreamingSource& s) {
        if (!s.active) return;
        if (auto p = position_of(e)) out.push_back(*p);
    });
    if (out.empty()) {
        world_.each<const Camera>([&](Entity e, const Camera& c) {
            if (!out.empty() || !c.active) return;
            if (auto p = position_of(e)) out.push_back(*p);
        });
    }
    return out;
}

std::optional<Vec3> WorldBridge::begin_origin_shift(std::span<const Vec3> sources) {
    if (sources.empty()) return std::nullopt;
    const auto offset = origin_.update(sources.front());
    if (offset) shift_roots(world_, *offset);
    return offset;
}

void WorldBridge::complete_origin_shift(Vec3 offset, std::vector<Vec3>& sources) {
    for (Vec3& p : sources) p = p - offset;
    if (map_) map_->set_origin(origin_.origin());
    world_.send(OriginShifted{offset, origin_.origin()});
}

void WorldBridge::stream(std::span<const Vec3> sources, bool finish) {
    if (!is_open()) return;
    stream_points_.clear();
    for (const Vec3& p : sources) {
        const WorldPosition w = origin_.to_world(p);
        stream_points_.push_back({w.x, w.z});
    }
    if (finish) map_->finish(stream_points_);
    else map_->update(stream_points_, static_cast<double>(stream_budget_ms_) / 1000.0);
}

std::optional<WorldCellsOverlay> WorldBridge::debug_cells() const {
    if (!is_open()) return std::nullopt;
    return WorldCellsOverlay{
        .map = &map_->map(),
        .origin = origin_.origin(),
        .attached_cells = map_->attached_cells(),
        .sources = streaming_sources(),
    };
}

} // namespace astral
