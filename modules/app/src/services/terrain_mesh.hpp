#pragma once
// Internal to Astral::App: terrain tile placement and stitched LOD mesh generation.

#include <astral/assets/terrain.hpp>
#include <astral/render/mesh.hpp>

#include <array>
#include <span>

namespace astral {

enum TerrainEdge : unsigned { TerrainWest, TerrainEast, TerrainNorth, TerrainSouth, TerrainEdgeCount };

struct TerrainMeshLod {
    unsigned stride = 1;
    unsigned stitched_edges = 0;
    std::array<std::span<const float>, TerrainEdgeCount> neighbors{};
};

struct TilePlacement {
    Vec3 corner;
    Vec3 centre;
};

[[nodiscard]] TilePlacement place_tile(const TerrainManifest& manifest, const TerrainTileSource& tile) noexcept;

/// Tile vertices are local to the minimum XZ corner; UVs span the terrain in meters.
[[nodiscard]] MeshData build_terrain_mesh(const TerrainManifest& manifest, const TerrainTileSource& tile,
                                          std::span<const float> heights, const TerrainMeshLod& lod = {});

} // namespace astral
