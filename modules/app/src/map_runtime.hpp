#pragma once
// Internal to Astral::App: an open map (ADR-0021). Reads cells (cooked files, or a source map cooked in
// memory for development), decodes them on the JobSystem, spawns them on the main thread within a
// time budget, keeps data layers and remembered changes.

#include <astral/world/map_state.hpp>
#include <astral/world/streaming.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astral {

class AssetManager;
class JobSystem;
class JobCounter;
class Logger;

class MapRuntime final : public CellLoader {
public:
    MapRuntime(World& world, PrefabLibrary& prefabs, AssetManager& assets, JobSystem& jobs, Logger& log);
    ~MapRuntime() override;

    /// Reads the map and spawns its always-loaded entities. False (logged) on a missing or wrong map.
    bool open(std::string_view path, const std::filesystem::path& asset_root);
    void close();
    [[nodiscard]] bool is_open() const noexcept { return streamer_ != nullptr; }

    /// Streams around `sources` (world coordinates); attaches ready cells for at most `budget_seconds`
    /// (at least one per call), refreshes layers when they changed.
    void update(std::span<const StreamingPoint> sources, double budget_seconds);
    /// Waits for every load in range and attaches all of them.
    void finish(std::span<const StreamingPoint> sources);

    void set_origin(const WorldPosition& origin) noexcept { state_.set_origin(origin); }
    [[nodiscard]] MapLayers* layers() noexcept { return layers_ ? &*layers_ : nullptr; }
    [[nodiscard]] const StreamingStats* stats() const noexcept { return streamer_ ? &streamer_->stats() : nullptr; }
    [[nodiscard]] const MapDesc& map() const noexcept { return map_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    /// Cells currently in the world (for debug drawing).
    [[nodiscard]] std::vector<CellKey> attached_cells() const;
    /// Resolves a persistent MapEntity::id into its current lifecycle status and live Entity (if Alive).
    [[nodiscard]] MapEntityLookup lookup(std::uint64_t id) const { return state_.lookup(world_, id); }

    // CellLoader
    void begin_load(CellKey cell) override;
    bool ready(CellKey cell) override;
    void attach(CellKey cell) override;
    void detach(CellKey cell) override;
    void cancel(CellKey cell) override;

private:
    struct Pending;
    struct KeyHash {
        std::size_t operator()(const CellKey& k) const noexcept {
            return (std::size_t{k.grid} * 73856093u) ^ (static_cast<std::size_t>(static_cast<std::uint32_t>(k.x)) * 19349663u) ^
                   (static_cast<std::size_t>(static_cast<std::uint32_t>(k.z)) * 83492791u);
        }
    };

    World& world_;
    PrefabLibrary& prefabs_;
    AssetManager& assets_;
    JobSystem& jobs_;
    Logger& log_;

    std::string path_;
    std::string cells_folder_; ///< "<stem>.cells/" (cooked maps)
    MapDesc map_;
    std::optional<MapLayers> layers_;
    std::uint64_t layers_seen_ = 0;
    MapState state_;
    std::unique_ptr<CellStreamer> streamer_;
    std::unordered_map<std::string, Json> memory_cells_; ///< Source maps: cooked in memory, by cell name.
    std::unordered_map<CellKey, std::shared_ptr<Pending>, KeyHash> pending_;
    std::vector<CellKey> attached_;
};

} // namespace astral
