#include "map_runtime.hpp"

#include "json_asset.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/world/level_instance.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>

namespace astral {

namespace {

constexpr CellKey always_cell{0xFFFFFFFFu, 0, 0};

std::vector<EntityFile> read_source_entities(const std::filesystem::path& folder, Logger& log) {
    std::vector<EntityFile> files;
    std::error_code ec;
    std::vector<std::filesystem::path> list;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".ent") list.push_back(entry.path());
    }
    std::sort(list.begin(), list.end());
    for (const auto& file : list) {
        std::ifstream in(file, std::ios::binary | std::ios::ate);
        if (!in) continue;
        const auto size = in.tellg();
        if (size < 0) continue;
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        in.seekg(0);
        if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), size)) continue;
        std::string parse_error;
        auto entity = parse_json_asset(data, &parse_error);
        if (!entity) {
            log.log(LogLevel::Error, "open_world " + file.string() + ": " + parse_error);
            continue;
        }
        files.push_back({file.filename().string(), std::move(*entity)});
    }
    return files;
}

} // namespace

/// A cell being read and decoded on a worker thread (ADR-0032).
struct MapRuntime::Pending {
    JobCounter counter;
    std::vector<MapEntity> entities;
    std::string error;
};

MapRuntime::MapRuntime(World& world, PrefabLibrary& prefabs, AssetManager& assets, JobSystem& jobs, Logger& log)
    : world_(world), prefabs_(prefabs), assets_(assets), jobs_(jobs), log_(log) {}

MapRuntime::~MapRuntime() {
    close();
}

bool MapRuntime::open(std::string_view path, const std::filesystem::path& asset_root) {
    auto fail = [&](const std::string& message) {
        log_.log(LogLevel::Error, "open_world " + std::string(path) + ": " + message);
        return false;
    };
    std::string error;
    auto bytes = assets_.read(path, &error);
    if (!bytes) return fail(error);
    auto doc = parse_json_asset(*bytes, &error);
    if (!doc) return fail(error);
    auto map = parse_map(*doc, &error);
    if (!map) return fail(error);

    std::string p(path);
    const std::string stem = p.ends_with(".world") ? p.substr(0, p.size() - 6) : p;
    std::vector<CellKey> keys;
    Json always;
    if (const Json* cells = doc->find("cells")) { // cooked index
        std::string next_cells_folder = stem + ".cells/";
        for (const Json& c : cells->as_array()) {
            keys.push_back({static_cast<std::uint32_t>(c["grid"].as_number()), static_cast<std::int32_t>(c["x"].as_number()),
                            static_cast<std::int32_t>(c["z"].as_number())});
        }
        auto always_bytes = assets_.read(next_cells_folder + "always.cell", &error);
        if (!always_bytes) return fail(error);
        auto decoded = parse_json_asset(*always_bytes, &error);
        if (!decoded) return fail(error);
        always = std::move(*decoded);
        close();
        cells_folder_ = std::move(next_cells_folder);
    } else { // source map: cook it in memory (development)
        const std::vector<EntityFile> files = read_source_entities(asset_root / (stem + ".entities"), log_);
        const MapSource raw_source = read_map_source(*map, files);
        const MapSource source = expand_map_level_instances(
            raw_source, prefabs_.types(), [&](std::string_view p) { return prefabs_.document(p); });
        for (const std::string& e : source.errors) log_.log(LogLevel::Error, "open_world " + std::string(path) + ": " + e);
        close();
        const CellAssignment assigned = assign_cells(*map, source.entities);
        for (const Cell& c : assigned.cells) {
            for (std::size_t idx : c.entities) state_.register_known(source.entities[idx].id, c.key);
        }
        for (std::size_t idx : assigned.always_loaded) state_.register_known(source.entities[idx].id, always_cell);
        CookedMap cooked = cook_map(source);
        for (CookedCell& cell : cooked.cells) {
            if (cell.name == "always") always = std::move(cell.document);
            else memory_cells_.emplace(cell.name, std::move(cell.document));
        }
        for (const Json& c : cooked.index["cells"].as_array()) {
            keys.push_back({static_cast<std::uint32_t>(c["grid"].as_number()), static_cast<std::int32_t>(c["x"].as_number()),
                            static_cast<std::int32_t>(c["z"].as_number())});
        }
    }

    path_ = p;
    map_ = *map;
    layers_.emplace(map_);
    layers_seen_ = layers_->version();
    const std::vector<MapEntity> always_entities = read_cell(map_, always);
    const LoadResult r = state_.spawn_cell(world_, prefabs_, always_cell, always_entities, *layers_);
    for (const std::string& e : r.errors) log_.log(LogLevel::Error, "open_world " + p + ": " + e);
    streamer_ = std::make_unique<CellStreamer>(map_, std::move(keys), *this);
    return true;
}

void MapRuntime::close() {
    if (!streamer_) return;
    streamer_->unload_all();
    state_.despawn_cell(world_, prefabs_, always_cell);
    for (auto& [key, pending] : pending_) jobs_.wait(pending->counter);
    pending_.clear();
    streamer_.reset();
    memory_cells_.clear();
    attached_.clear();
    layers_.reset();
    state_ = MapState{};
    path_.clear();
}

void MapRuntime::update(std::span<const StreamingPoint> sources, double budget_seconds) {
    if (!streamer_) return;
    if (layers_->version() != layers_seen_) {
        layers_seen_ = layers_->version();
        state_.refresh_layers(world_, prefabs_, *layers_);
    }
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    streamer_->update(sources, 1);
    while (streamer_->stats().waiting > 0 &&
           std::chrono::duration<double>(clock::now() - start).count() < budget_seconds) {
        streamer_->update(sources, 1);
    }
}

void MapRuntime::finish(std::span<const StreamingPoint> sources) {
    if (!streamer_) return;
    streamer_->update(sources, 0); // start every load in range
    for (auto& [key, pending] : pending_) jobs_.wait(pending->counter);
    streamer_->update(sources, 0xFFFFFFFFu);
}

std::vector<CellKey> MapRuntime::attached_cells() const {
    return attached_;
}

void MapRuntime::begin_load(CellKey cell) {
    const std::string name = cell_name(map_, cell);
    auto pending = std::make_shared<Pending>();
    if (auto it = memory_cells_.find(name); it != memory_cells_.end()) {
        pending->entities = read_cell(map_, it->second);
        pending_[cell] = std::move(pending);
        return;
    }
    const std::string file = cells_folder_ + name + ".cell";
    AssetManager* assets = &assets_;
    const MapDesc* map = &map_;
    Pending* job = pending.get();
    jobs_.submit(pending->counter, [job, assets, map, file] {
        auto bytes = assets->read(file, &job->error);
        if (!bytes) return;
        auto doc = parse_json_asset(*bytes, &job->error);
        if (doc) job->entities = read_cell(*map, *doc);
    });
    pending_[cell] = std::move(pending);
}

bool MapRuntime::ready(CellKey cell) {
    auto it = pending_.find(cell);
    return it != pending_.end() && it->second->counter.done();
}

void MapRuntime::attach(CellKey cell) {
    auto it = pending_.find(cell);
    if (it == pending_.end()) return;
    std::shared_ptr<Pending> pending = std::move(it->second);
    pending_.erase(it);
    jobs_.wait(pending->counter);
    if (!pending->error.empty()) log_.log(LogLevel::Error, "world cell " + cell_name(map_, cell) + ": " + pending->error);
    const LoadResult r = state_.spawn_cell(world_, prefabs_, cell, pending->entities, *layers_);
    for (const std::string& e : r.errors) log_.log(LogLevel::Error, "world cell " + cell_name(map_, cell) + ": " + e);
    attached_.push_back(cell);
}

void MapRuntime::detach(CellKey cell) {
    state_.despawn_cell(world_, prefabs_, cell);
    std::erase(attached_, cell);
}

void MapRuntime::cancel(CellKey cell) {
    auto it = pending_.find(cell);
    if (it == pending_.end()) return;
    jobs_.wait(it->second->counter); // a decode in flight finishes first (it only touches its Pending)
    pending_.erase(it);
}

} // namespace astral
