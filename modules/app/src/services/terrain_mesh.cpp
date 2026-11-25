#include "terrain_mesh.hpp"

namespace astral {

namespace {

float boundary_slope(const TerrainManifest& manifest, std::span<const float> heights, const TerrainMeshLod& lod,
                     unsigned x, unsigned z, bool along_z) noexcept {
    const std::uint32_t n = manifest.samples;
    const unsigned edge = n - 1;
    const unsigned coordinate = along_z ? z : x;
    const auto before = lod.neighbors[along_z ? TerrainNorth : TerrainWest];
    const auto after = lod.neighbors[along_z ? TerrainSouth : TerrainEast];
    const auto height = [&](unsigned at) noexcept { return along_z ? heights[at * n + x] : heights[z * n + at]; };
    const bool has_before = coordinate != 0 || !before.empty();
    const bool has_after = coordinate != edge || !after.empty();
    const float low = coordinate != 0
                          ? height(coordinate - 1)
                          : (has_before ? before[along_z ? (edge - 1) * n + x : z * n + edge - 1] : height(0));
    const float high = coordinate != edge
                           ? height(coordinate + 1)
                           : (has_after ? after[along_z ? n + x : z * n + 1] : height(edge));
    return (high - low) / (has_before && has_after ? 2.0f * manifest.spacing_m : manifest.spacing_m);
}

} // namespace

TilePlacement place_tile(const TerrainManifest& manifest, const TerrainTileSource& tile) noexcept {
    const double side = static_cast<double>(manifest.samples - 1) * manifest.spacing_m;
    return {{static_cast<float>(tile.x * side), 0, static_cast<float>(tile.z * side)},
            {static_cast<float>((static_cast<double>(tile.x) + 0.5) * side), 0,
             static_cast<float>((static_cast<double>(tile.z) + 0.5) * side)}};
}

MeshData build_terrain_mesh(const TerrainManifest& manifest, const TerrainTileSource& tile,
                            std::span<const float> heights, const TerrainMeshLod& lod) {
    const std::uint32_t n = manifest.samples;
    const unsigned edge = n - 1;
    const unsigned cells = edge / lod.stride;
    const unsigned columns = cells + 1;
    const float step = manifest.spacing_m;
    const float inv_two_step = 1.0f / (2.0f * step);
    const Vec3 corner = place_tile(manifest, tile).corner;

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(columns) * columns);
    mesh.indices.reserve(static_cast<std::size_t>(cells) * cells * 6);

    for (unsigned z = 0; z < n; z += lod.stride) {
        const std::size_t row = static_cast<std::size_t>(z) * n;
        const float pz = static_cast<float>(z) * step;
        const bool interior_z = z > 0 && z < edge;
        for (unsigned x = 0; x < n; x += lod.stride) {
            const float px = static_cast<float>(x) * step;
            float sx;
            float sz;
            if (interior_z && x > 0 && x < edge) {
                sx = (heights[row + x + 1] - heights[row + x - 1]) * inv_two_step;
                sz = (heights[row + n + x] - heights[row - n + x]) * inv_two_step;
            } else {
                sx = boundary_slope(manifest, heights, lod, x, z, false);
                sz = boundary_slope(manifest, heights, lod, x, z, true);
            }
            mesh.vertices.push_back(
                {{px, heights[row + x], pz}, normalize(Vec3{-sx, 1.0f, -sz}), {corner.x + px, corner.z + pz}});
        }
    }

    const auto index = [&](unsigned x, unsigned z) noexcept {
        const bool stitch_z = (x == 0 && (lod.stitched_edges & (1u << TerrainWest))) ||
                              (x == cells && (lod.stitched_edges & (1u << TerrainEast)));
        const bool stitch_x = (z == 0 && (lod.stitched_edges & (1u << TerrainNorth))) ||
                              (z == cells && (lod.stitched_edges & (1u << TerrainSouth)));
        return (stitch_z ? z & ~1u : z) * columns + (stitch_x ? x & ~1u : x);
    };
    const auto triangle = [&](unsigned a, unsigned b, unsigned c) {
        if (a != b && b != c && a != c) {
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        }
    };
    for (unsigned z = 0; z < cells; ++z) {
        for (unsigned x = 0; x < cells; ++x) {
            const unsigned a = index(x, z);
            const unsigned d = index(x + 1, z + 1);
            triangle(a, index(x, z + 1), d);
            triangle(a, d, index(x + 1, z));
        }
    }
    return mesh;
}

} // namespace astral
