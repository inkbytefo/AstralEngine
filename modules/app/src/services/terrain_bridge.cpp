#include "terrain_bridge.hpp"

#include "render_bridge.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>

#include <algorithm>
#include <cmath>

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
                            std::span<const float> heights) {
    const std::uint32_t n = manifest.samples;
    const float step = manifest.spacing_m;
    const Vec3 corner = place_tile(manifest, tile).corner;
    MeshData mesh;
    mesh.vertices.reserve(heights.size());
    const std::size_t intervals = n - 1;
    mesh.indices.reserve(intervals * intervals * 6);
    for (std::uint32_t z = 0; z < n; ++z) {
        const auto north = z == 0 ? z : z - 1;
        const auto south = std::min(z + 1, n - 1);
        for (std::uint32_t x = 0; x < n; ++x) {
            const auto west = x == 0 ? x : x - 1;
            const auto east = std::min(x + 1, n - 1);
            const float dx = (heights[z * n + east] - heights[z * n + west]) / (static_cast<float>(east - west) * step);
            const float dz = (heights[south * n + x] - heights[north * n + x]) / (static_cast<float>(south - north) * step);
            const float px = static_cast<float>(x) * step;
            const float pz = static_cast<float>(z) * step;
            mesh.vertices.push_back({{px, heights[z * n + x], pz}, normalize(Vec3{-dx, 1, -dz}),
                                     {corner.x + px, corner.z + pz}});
        }
    }
    for (std::uint32_t z = 0; z + 1 < n; ++z) {
        for (std::uint32_t x = 0; x + 1 < n; ++x) {
            const std::uint32_t a = z * n + x;
            const std::uint32_t d = a + n + 1;
            // Match the diagonal and winding used by Jolt's heightfield.
            mesh.indices.insert(mesh.indices.end(), {a, a + n, d, a, d, a + 1});
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
        if (renderer_) tile.mesh = renderer_->create_mesh(build_terrain_mesh(*manifest, source, heights));
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
        if (existing == instances_.end()) pending.push_back({owner, source, {}});
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

} // namespace astral
