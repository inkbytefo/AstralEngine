#include "terrain_bridge.hpp"

#include "render_bridge.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace astral {
namespace {
struct TilePlacement {
    Vec3 corner;
    Vec3 centre;
};

TilePlacement place_tile(const TerrainManifest& manifest, const TerrainTileSource& tile) {
    const double side = static_cast<double>(manifest.samples - 1) * manifest.spacing_m;
    return {{static_cast<float>(tile.x * side), 0, static_cast<float>(tile.z * side)},
            {static_cast<float>((static_cast<double>(tile.x) + 0.5) * side), 0,
             static_cast<float>((static_cast<double>(tile.z) + 0.5) * side)}};
}

bool supports_terrain_transform(const World& world, Entity entity, const Transform& transform) {
    const Vec3 p = transform.position;
    return !world.has<Parent>(entity) && transform.rotation == Quat{} && transform.scale == Vec3{1, 1, 1} &&
           std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
} // namespace

MeshData build_terrain_mesh(const TerrainManifest& manifest, const TerrainTileSource& tile,
                            std::span<const float> heights, const TerrainMeshLod& lod) {
    const std::uint32_t n = manifest.samples;
    const unsigned edge = n - 1;
    const unsigned cells = edge / lod.stride;
    const unsigned columns = cells + 1;
    const float step = manifest.spacing_m;
    const Vec3 corner = place_tile(manifest, tile).corner;
    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(columns) * columns);
    mesh.indices.reserve(static_cast<std::size_t>(cells) * cells * 6);

    const auto normal_axis = [&](unsigned x, unsigned z, bool along_z) {
        const unsigned coordinate = along_z ? z : x;
        const auto before = lod.neighbors[along_z ? TerrainNorth : TerrainWest];
        const auto after = lod.neighbors[along_z ? TerrainSouth : TerrainEast];
        const auto height = [&](unsigned at) { return along_z ? heights[at * n + x] : heights[z * n + at]; };
        const bool has_before = coordinate != 0 || !before.empty();
        const bool has_after = coordinate != edge || !after.empty();
        const float low = coordinate != 0 ? height(coordinate - 1) :
            (has_before ? before[along_z ? (edge - 1) * n + x : z * n + edge - 1] : height(0));
        const float high = coordinate != edge ? height(coordinate + 1) :
            (has_after ? after[along_z ? n + x : z * n + 1] : height(edge));
        return (high - low) / (has_before && has_after ? 2.0f * step : step);
    };
    for (unsigned z = 0; z < n; z += lod.stride) {
        for (unsigned x = 0; x < n; x += lod.stride) {
            const float px = static_cast<float>(x) * step;
            const float pz = static_cast<float>(z) * step;
            mesh.vertices.push_back({{px, heights[z * n + x], pz},
                                     normalize(Vec3{-normal_axis(x, z, false), 1, -normal_axis(x, z, true)}),
                                     {corner.x + px, corner.z + pz}});
        }
    }

    const auto index = [&](unsigned x, unsigned z) {
        const bool stitch_z = (x == 0 && (lod.stitched_edges & (1u << TerrainWest))) ||
                              (x == cells && (lod.stitched_edges & (1u << TerrainEast)));
        const bool stitch_x = (z == 0 && (lod.stitched_edges & (1u << TerrainNorth))) ||
                              (z == cells && (lod.stitched_edges & (1u << TerrainSouth)));
        // Collapse odd boundary vertices onto the previous coarse vertex.
        return (stitch_z ? z & ~1u : z) * columns + (stitch_x ? x & ~1u : x);
    };
    const auto triangle = [&](unsigned a, unsigned b, unsigned c) {
        if (a != b && b != c && a != c) mesh.indices.insert(mesh.indices.end(), {a, b, c});
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

TerrainBridge::TerrainBridge(World& world, AssetManager& assets, Logger& log, RenderBridge& render,
                             Renderer* renderer, PhysicsWorld& physics)
    : world_(world), assets_(assets), log_(log), render_(render), renderer_(renderer), physics_(physics) {}

TerrainBridge::~TerrainBridge() {
    for (auto& instance : instances_) release(instance);
}

void TerrainBridge::release(Instance& instance) {
    for (const auto& tile : instance.tiles) {
        if (world_.alive(tile.draw)) world_.destroy(tile.draw);
        if (tile.mesh.valid()) renderer_->destroy(tile.mesh);
        if (tile.body.valid()) physics_.destroy(tile.body);
    }
    instance.tiles.clear();
}

void TerrainBridge::load(Instance& instance, Vec3 origin) {
    std::string error;
    const auto manifest_bytes = assets_.read(instance.source.asset, &error);
    const auto manifest = manifest_bytes ? read_terrain_manifest(*manifest_bytes, &error) : std::nullopt;
    if (!manifest) {
        log_.warn("terrain {}: {}", instance.source.asset, error);
        return;
    }
    instance.manifest = *manifest;
    const MaterialHandle material = renderer_ && !instance.source.material_file.empty() ?
        render_.load_material(instance.source.material_file) : MaterialHandle{};
    for (const auto& source : manifest->tiles) {
        const auto bytes = assets_.read(source.file, &error);
        const auto samples = bytes ? read_terrain_tile(*bytes, manifest->samples, &error) : std::nullopt;
        if (!samples) {
            log_.warn("terrain tile {}: {}", source.file, error);
            release(instance);
            return;
        }
        const auto placement = place_tile(*manifest, source);
        const Vec3 position = origin + placement.centre;
        if (!std::isfinite(position.x) || !std::isfinite(position.z)) {
            log_.warn("terrain tile {}: position exceeds the supported coordinate range", source.file);
            release(instance);
            return;
        }
        std::vector<float> heights;
        heights.reserve(samples->size());
        for (const auto sample : *samples) heights.push_back(manifest->height_m(sample));
        Tile& tile = instance.tiles.emplace_back();
        tile.offset = placement.centre;
        if (renderer_) {
            tile.source = source;
            tile.heights = heights;
            tile.mesh = renderer_->create_mesh(build_terrain_mesh(*manifest, source, heights));
        }
        tile.body = physics_.create_body({.shape = ShapeDesc::height_field(std::move(heights), manifest->samples, manifest->spacing_m),
                                          .motion = MotionType::Static, .position = position,
                                          .user_data = to_user_data(instance.owner)});
        if (!tile.body.valid() || (renderer_ && !tile.mesh.valid())) {
            log_.warn("terrain tile {}: could not create render or collision resources", source.file);
            release(instance);
            return;
        }
        if (tile.mesh.valid()) {
            tile.draw = world_.create(TerrainTileDraw{}, Parent{instance.owner}, Transform{.position = placement.corner},
                                      MeshRenderer{tile.mesh, material});
        }
    }
}

void TerrainBridge::sync() {
    std::erase_if(instances_, [&](Instance& instance) {
        const auto* ref = world_.get<const TerrainRef>(instance.owner);
        const auto* transform = world_.get<const Transform>(instance.owner);
        if (ref && transform && *ref == instance.source && supports_terrain_transform(world_, instance.owner, *transform)) {
            return false;
        }
        release(instance);
        return true;
    });

    std::vector<Instance> pending;
    world_.each<const TerrainRef, const Transform>([&](Entity owner, const TerrainRef& source, const Transform& transform) {
        if (source.asset.empty()) return;
        const auto existing = std::find_if(instances_.begin(), instances_.end(), [owner](const Instance& instance) {
            return instance.owner == owner;
        });
        if (!supports_terrain_transform(world_, owner, transform)) {
            log_.warn("terrain {}: expected a finite translation on a root entity with unit scale", source.asset);
            return;
        }
        if (existing == instances_.end()) pending.push_back({owner, source, {}, {}});
    });
    for (auto& instance : pending) {
        load(instance, world_.get<const Transform>(instance.owner)->position);
        instances_.push_back(std::move(instance));
    }
    for (const auto& instance : instances_) {
        const Vec3 origin = world_.get<const Transform>(instance.owner)->position;
        for (const auto& tile : instance.tiles) {
            const Vec3 position = origin + tile.offset;
            if (position != physics_.position(tile.body)) physics_.set_transform(tile.body, position, {});
        }
    }
}

void TerrainBridge::update_lod() {
    if (!renderer_ || instances_.empty()) return;
    const Vec3 camera = render_.camera_position();
    constexpr std::array<std::pair<int, int>, TerrainEdgeCount> directions{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
    for (auto& instance : instances_) {
        if (instance.tiles.empty()) continue;
        const auto* transform = world_.get<const Transform>(instance.owner);
        if (!transform) continue;
        const auto& manifest = instance.manifest;
        const float side = static_cast<float>(manifest.samples - 1) * manifest.spacing_m;
        std::map<std::pair<std::int64_t, std::int64_t>, std::size_t> addresses;
        std::vector<TerrainMeshLod> choices(instance.tiles.size());
        for (std::size_t i = 0; i < instance.tiles.size(); ++i) {
            const auto& tile = instance.tiles[i];
            addresses.emplace(std::pair{tile.source.x, tile.source.z}, i);
            const Vec3 delta = transform->position + tile.offset - camera;
            const float distance = std::hypot(delta.x, delta.z);
            const float threshold = side * (tile.stride == 1 ? 1.25f : 1.05f);
            choices[i].stride = distance > threshold ? 2u : 1u;
        }
        std::vector<std::pair<std::size_t, MeshHandle>> replacements;
        for (std::size_t i = 0; i < instance.tiles.size(); ++i) {
            auto& choice = choices[i];
            const auto& tile = instance.tiles[i];
            for (unsigned edge = 0; edge < TerrainEdgeCount; ++edge) {
                const auto [dx, dz] = directions[edge];
                const auto neighbor = addresses.find({static_cast<std::int64_t>(tile.source.x) + dx,
                                                      static_cast<std::int64_t>(tile.source.z) + dz});
                if (neighbor == addresses.end()) continue;
                choice.neighbors[edge] = instance.tiles[neighbor->second].heights;
                if (choices[neighbor->second].stride > choice.stride) choice.stitched_edges |= 1u << edge;
            }
            if (choice.stride == tile.stride && choice.stitched_edges == tile.stitched_edges &&
                (tile.has_shared_normals || std::all_of(choice.neighbors.begin(), choice.neighbors.end(),
                                                       [](const auto samples) { return samples.empty(); }))) continue;
            const auto mesh = renderer_->create_mesh(build_terrain_mesh(manifest, tile.source, tile.heights, choice));
            if (!mesh.valid()) {
                for (const auto& replacement : replacements) renderer_->destroy(replacement.second);
                replacements.clear();
                break;
            }
            replacements.emplace_back(i, mesh);
        }
        // Commit the whole transition together so a failed upload cannot leave a mismatched edge.
        for (const auto& [i, mesh] : replacements) {
            auto& tile = instance.tiles[i];
            auto* draw = world_.get<MeshRenderer>(tile.draw);
            if (!draw) {
                renderer_->destroy(mesh);
                continue;
            }
            draw->mesh = mesh;
            renderer_->destroy(tile.mesh);
            tile.mesh = mesh;
            tile.stride = choices[i].stride;
            tile.stitched_edges = choices[i].stitched_edges;
            tile.has_shared_normals = true;
        }
    }
}

} // namespace astral
