#pragma once

#include "terrain_mesh.hpp"

#include <astral/app/components.hpp>
#include <astral/assets/terrain.hpp>
#include <astral/core/jobs.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astral {

class AssetManager;
class Logger;
class RenderBridge;
class Profiler;

struct TerrainTileDraw {};

class TerrainBridge {
public:
    TerrainBridge(World& world, AssetManager& assets, Logger& log, RenderBridge& render,
                  Renderer* renderer, PhysicsWorld& physics, JobSystem& jobs, Profiler& profiler);
    ~TerrainBridge();

    void sync(std::span<const Vec3> sources, bool finish = false);
    [[nodiscard]] bool streaming_enabled() const;
    [[nodiscard]] bool character_tiles_ready(Vec3 from, Vec3 to, float radius) const;
    void update_lod();

private:
    struct TileCoord {
        std::int32_t x = 0;
        std::int32_t z = 0;
        bool operator==(const TileCoord&) const noexcept = default;
    };
    struct TileCoordHash {
        [[nodiscard]] std::size_t operator()(const TileCoord& c) const noexcept {
            return (static_cast<std::size_t>(static_cast<std::uint32_t>(c.x)) * 73856093u) ^
                   (static_cast<std::size_t>(static_cast<std::uint32_t>(c.z)) * 19349663u);
        }
    };
    struct TileLoad {
        std::atomic<bool> ready{false};
        std::string error;
        std::vector<float> heights;
        MeshData base_mesh;
    };
    struct Tile {
        Entity draw{};
        MeshHandle mesh{};
        BodyHandle body{};
        Vec3 offset{};
        TerrainTileSource source;
        std::vector<float> heights;
        MeshData pending_mesh;
        unsigned stride = 1;
        unsigned stitched_edges = 0;
        unsigned neighbor_edges = 0;
        bool failed = false;
        std::shared_ptr<TileLoad> request;
    };
    struct Instance {
        Entity owner{};
        TerrainRef source;
        std::vector<Tile> tiles;
        std::unordered_map<TileCoord, std::size_t, TileCoordHash> resident_meshes;
        TerrainManifest manifest;
        MaterialHandle material{};
        Vec3 last_lod_camera{};
        Vec3 last_lod_origin{};
        bool has_lod_state = false;
        bool lod_dirty = true;
    };
    struct WantedTile {
        std::size_t index = 0;
        float distance = 0.0f;
        bool render = false;
        bool collision = false;
    };

    void load(Instance& instance);
    void sync_tiles(Instance& instance, Vec3 origin, std::span<const Vec3> sources,
                    std::span<const Vec3> characters, bool finish);
    bool prepare_tile(Instance& instance, Tile& tile, bool incremental);
    void publish_tile(Instance& instance, std::size_t tile_idx, Vec3 centre, bool render, bool collision);
    void read_tile(TileLoad& result, const TerrainManifest& manifest, const TerrainTileSource& source) const;
    void release_tile(Instance& instance, Tile& tile, bool render, bool collision);
    void commit_lod_replacements(Instance& instance);
    void update_instance_lod(Instance& instance, const Vec3& camera);
    void release(Instance& instance);

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    RenderBridge& render_;
    Renderer* renderer_;
    PhysicsWorld& physics_;
    JobSystem& jobs_;
    Profiler& profiler_;
    JobCounter loads_;
    std::atomic<unsigned> pending_loads_{0};
    std::vector<Instance> instances_;
    std::vector<WantedTile> wanted_scratch_;
    std::vector<Vec3> characters_scratch_;
    std::vector<TerrainMeshLod> lod_choices_;
    std::vector<std::pair<std::size_t, MeshHandle>> lod_replacements_;
};

} // namespace astral
