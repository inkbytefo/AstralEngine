#pragma once
// Internal to Astral::App: owns open-map streaming (MapRuntime), floating origin, and scene/prefab I/O.

#include <astral/world/map_state.hpp>
#include <astral/world/origin.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/streaming.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace astral {

class AssetManager;
class JobSystem;
class Logger;
class MapRuntime;
class World;
struct DirectionalLight;
struct Environment;

/// Snapshot of open-map cell state passed to DevTools for debug overlay drawing.
struct WorldCellsOverlay {
    const MapDesc* map = nullptr;
    WorldPosition origin{};
    std::vector<CellKey> attached_cells;
    std::vector<Vec3> sources;
};

class WorldBridge {
public:
    WorldBridge(World& world, PrefabLibrary& prefabs, AssetManager& assets, JobSystem& jobs, Logger& log,
                std::filesystem::path asset_root, float origin_shift_distance, float stream_budget_ms);
    ~WorldBridge();
    WorldBridge(const WorldBridge&) = delete;
    WorldBridge& operator=(const WorldBridge&) = delete;

    LoadResult load_scene(std::string_view path, DirectionalLight& sun, Environment& environment);
    bool save_scene(std::string_view path, const DirectionalLight& sun, const Environment& environment);
    Entity spawn_prefab(std::string_view path, const Transform& transform);
    void sync_prefabs();

    bool open_world(std::string_view path);
    void close_world();
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] MapLayers* layers() noexcept;
    [[nodiscard]] const StreamingStats* streaming() const noexcept;
    [[nodiscard]] WorldPosition origin() const noexcept { return origin_.origin(); }
    [[nodiscard]] MapEntityLookup resolve_map_entity(std::uint64_t id) const;

    [[nodiscard]] std::vector<Vec3> streaming_sources() const;
    [[nodiscard]] std::optional<Vec3> begin_origin_shift(std::span<const Vec3> sources);
    void complete_origin_shift(Vec3 offset, std::vector<Vec3>& sources);
    void stream(std::span<const Vec3> sources, bool finish);
    [[nodiscard]] std::optional<WorldCellsOverlay> debug_cells() const;

private:
    void log_problems(std::string_view what, const std::vector<std::string>& problems) const;

    World& world_;
    PrefabLibrary& prefabs_;
    AssetManager& assets_;
    JobSystem& jobs_;
    Logger& log_;
    std::filesystem::path asset_root_;
    float stream_budget_ms_;
    std::unique_ptr<MapRuntime> map_;
    FloatingOrigin origin_;
    std::vector<StreamingPoint> stream_points_;
};

} // namespace astral
