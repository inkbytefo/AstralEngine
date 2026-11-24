#pragma once

#include <astral/app/components.hpp>
#include <astral/assets/terrain.hpp>

#include <array>
#include <vector>

namespace astral {

class AssetManager;
class Logger;
class RenderBridge;

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
                  Renderer* renderer, PhysicsWorld& physics);
    ~TerrainBridge();

    void sync();
    void update_lod();

private:
    struct Tile {
        Entity draw{};
        MeshHandle mesh{};
        BodyHandle body{};
        Vec3 offset{};
        TerrainTileSource source;
        std::vector<float> heights;
        unsigned stride = 1;
        unsigned stitched_edges = 0;
        bool has_shared_normals = false;
    };
    struct Instance {
        Entity owner{};
        TerrainRef source;
        std::vector<Tile> tiles;
        TerrainManifest manifest;
    };

    void load(Instance& instance, Vec3 origin);
    void release(Instance& instance);

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    RenderBridge& render_;
    Renderer* renderer_;
    PhysicsWorld& physics_;
    std::vector<Instance> instances_;
};

} // namespace astral
