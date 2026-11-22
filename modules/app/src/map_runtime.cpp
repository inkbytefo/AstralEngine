#include "map_runtime.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>

namespace astral {

namespace {

constexpr CellKey always_cell{0xFFFFFFFFu, 0, 0};

std::optional<Json> decode(const std::vector<std::uint8_t>& bytes, std::string* error) {
    if (Json::is_binary(bytes)) return Json::from_binary(bytes, error);
    return Json::parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), error);
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
    close();
    auto fail = [&](const std::string& message) {
        log_.log(LogLevel::Error, "open_world " + std::string(path) + ": " + message);
        return false;
    };
    std::string error;
    auto bytes = assets_.read(path, &error);
    if (!bytes) return fail(error);
    auto doc = decode(*bytes, &error);
    if (!doc) return fail(error);
    auto map = parse_map(*doc, &error);
    if (!map) return fail(error);

    std::string p(path);
    const std::string stem = p.ends_with(".world") ? p.substr(0, p.size() - 6) : p;
    std::vector<CellKey> keys;
    Json always;
    if (const Json* cells = doc->find("cells")) { // cooked index
        cells_folder_ = stem + ".cells/";
        for (const Json& c : cells->as_array()) {
            keys.push_back({static_cast<std::uint32_t>(c["grid"].as_number()), static_cast<std::int32_t>(c["x"].as_number()),
                            static_cast<std::int32_t>(c["z"].as_number())});
        }
        auto always_bytes = assets_.read(cells_folder_ + "always.cell", &error);
        if (!always_bytes) return fail(error);
        auto decoded = decode(*always_bytes, &error);
        if (!decoded) return fail(error);
        always = std::move(*decoded);
    } else { // source map: cook it in memory (development)
        std::vector<EntityFile> files;
        const std::filesystem::path folder = asset_root / (stem + ".entities");
        std::error_code ec;
        std::vector<std::filesystem::path> list;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ent") list.push_back(entry.path());
        }
        std::sort(list.begin(), list.end());
        for (const auto& file : list) {
            std::ifstream in(file, std::ios::binary);
            std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string parse_error;
            auto entity = decode(data, &parse_error);
            if (!entity) {
                log_.log(LogLevel::Error, "open_world " + file.string() + ": " + parse_error);
                continue;
            }
            files.push_back({file.filename().string(), std::move(*entity)});
        }
        const MapSource source = read_map_source(*map, files);
        for (const std::string& e : source.errors) log_.log(LogLevel::Error, "open_world " + std::string(path) + ": " + e);
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
        auto doc = decode(*bytes, &job->error);
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
