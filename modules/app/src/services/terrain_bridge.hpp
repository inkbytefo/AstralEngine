#pragma once

#include <astral/app/components.hpp>
#include <astral/assets/terrain.hpp>

#include <astral/core/jobs.hpp>

#include <array>
#include <memory>
#include <vector>

namespace astral {

class AssetManager;
class Logger;
class RenderBridge;
class Profiler;

struct TerrainTileDraw {};

enum TerrainEdge : unsigned { TerrainWest, TerrainEast, TerrainNorth, TerrainSouth, TerrainEdgeCount };

struct TerrainMeshLod {
    unsigned stride = 1;
    unsigned stitched_edges = 0;
    std::array<std::span<const float>, TerrainEdgeCount> neighbors{};
};

/// Tile vertices are local to the minimum XZ corner; UVs span the terrain in meters.
MeshData build_terrain_mesh(const TerrainManifest& manifest, const TerrainTileSource& tile,
                            std::span<const float> heights, const TerrainMeshLod& lod = {});

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
        TerrainManifest manifest;
        MaterialHandle material{};
    };

    void load(Instance& instance);
    void sync_tiles(Instance& instance, Vec3 origin, std::span<const Vec3> sources,
                    std::span<const Vec3> characters, bool finish);
    bool prepare_tile(Instance& instance, Tile& tile, bool incremental);
    void publish_tile(Instance& instance, Tile& tile, Vec3 centre, bool render, bool collision);
    void read_tile(TileLoad& result, const TerrainManifest& manifest, const TerrainTileSource& source) const;
    void release_tile(Tile& tile, bool render, bool collision);
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
};

} // namespace astral
