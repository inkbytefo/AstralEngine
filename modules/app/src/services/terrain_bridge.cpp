#include "terrain_bridge.hpp"

#include "render_bridge.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <limits>

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
                             Renderer* renderer, PhysicsWorld& physics, JobSystem& jobs, Profiler& profiler)
    : world_(world), assets_(assets), log_(log), render_(render), renderer_(renderer), physics_(physics), jobs_(jobs), profiler_(profiler) {}

TerrainBridge::~TerrainBridge() {
    // Jobs hold the asset service and their own result storage, never ECS or renderer handles.
    jobs_.wait(loads_);
    for (auto& instance : instances_) release(instance);
}

void TerrainBridge::release_tile(Tile& tile, bool render, bool collision) {
    if (render && tile.mesh.valid()) {
        if (world_.alive(tile.draw)) world_.destroy(tile.draw);
        renderer_->destroy(tile.mesh);
        tile.draw = {};
        tile.mesh = {};
        tile.stride = 1;
        tile.stitched_edges = 0;
        tile.neighbor_edges = 0;
    }
    if (collision && tile.body.valid()) {
        physics_.destroy(tile.body);
        tile.body = {};
    }
}

void TerrainBridge::release(Instance& instance) {
    for (auto& tile : instance.tiles) release_tile(tile, true, true);
    instance.tiles.clear();
}

void TerrainBridge::load(Instance& instance) {
    std::string error;
    const auto bytes = assets_.read(instance.source.asset, &error);
    auto manifest = bytes ? read_terrain_manifest(*bytes, &error) : std::nullopt;
    if (!manifest) {
        log_.warn("terrain {}: {}", instance.source.asset, error);
        return;
    }
    instance.manifest = std::move(*manifest);
    if (renderer_ && !instance.source.material_file.empty()) instance.material = render_.load_material(instance.source.material_file);
    for (const auto& source : instance.manifest.tiles) {
        Tile& tile = instance.tiles.emplace_back();
        tile.source = source;
        tile.offset = place_tile(instance.manifest, source).centre;
    }
}

void TerrainBridge::read_tile(TileLoad& result, const TerrainManifest& manifest, const TerrainTileSource& source) const {
    const auto bytes = assets_.read(source.file, &result.error);
    const auto samples = bytes ? read_terrain_tile(*bytes, manifest.samples, &result.error) : std::nullopt;
    if (!samples) return;
    result.heights.reserve(samples->size());
    for (const auto sample : *samples) result.heights.push_back(manifest.height_m(sample));
    if (renderer_) result.base_mesh = build_terrain_mesh(manifest, source, result.heights);
}

namespace {
float distance_to_tile(Vec3 point, Vec3 centre, float half) {
    const Vec3 delta = point - centre;
    return std::hypot(std::max(0.0f, std::abs(delta.x) - half), std::max(0.0f, std::abs(delta.z) - half));
}

float nearest_tile_distance(std::span<const Vec3> points, Vec3 centre, float half) {
    float distance = std::numeric_limits<float>::infinity();
    for (const Vec3 point : points) distance = std::min(distance, distance_to_tile(point, centre, half));
    return distance;
}

bool streams(const TerrainRef& ref) {
    return ref.render_radius_m > 0 || ref.collision_radius_m > 0;
}
} // namespace

void TerrainBridge::sync_tiles(Instance& instance, Vec3 origin, std::span<const Vec3> sources,
                               std::span<const Vec3> characters, bool finish) {
    if (instance.tiles.empty()) return;
    const auto& manifest = instance.manifest;
    const float half = static_cast<float>(manifest.samples - 1) * manifest.spacing_m * 0.5f;
    const float collision_radius = instance.source.collision_radius_m > 0 ?
        std::max(instance.source.collision_radius_m, instance.source.render_radius_m) : 0.0f;
    const bool incremental = streams(instance.source) && !finish;
    struct Wanted {
        Tile* tile;
        float distance;
        bool render;
        bool collision;
    };
    std::vector<Wanted> wanted;
    for (auto& tile : instance.tiles) {
        const Vec3 centre = origin + tile.offset;
        if (!std::isfinite(centre.x) || !std::isfinite(centre.z)) continue;
        const float distance = nearest_tile_distance(sources, centre, half);
        const float character_distance = nearest_tile_distance(characters, centre, half);
        const bool render = renderer_ && (instance.source.render_radius_m <= 0 ||
            distance <= instance.source.render_radius_m + (tile.mesh.valid() ? half * 0.5f : 0.0f));
        const bool collision = collision_radius <= 0 || std::min(distance, character_distance) <=
            collision_radius + (tile.body.valid() ? half * 0.5f : 0.0f);
        release_tile(tile, !render, !collision);
        if (!render && !collision) {
            tile.request.reset();
            std::vector<float>().swap(tile.heights);
            tile.failed = false;
            continue;
        }
        wanted.push_back({&tile, std::min(distance, character_distance), render, collision});
    }
    std::stable_sort(wanted.begin(), wanted.end(), [](const Wanted& a, const Wanted& b) { return a.distance < b.distance; });

    unsigned publications = 0;
    for (const auto& entry : wanted) {
        auto& tile = *entry.tile;
        const Vec3 centre = origin + tile.offset;
        if (tile.body.valid() && centre != physics_.position(tile.body)) physics_.set_transform(tile.body, centre, {});
        if (tile.failed || ((!entry.render || tile.mesh.valid()) && (!entry.collision || tile.body.valid()))) continue;
        if (incremental && publications >= 4) continue;
        if (!prepare_tile(instance, tile, incremental)) continue;
        publish_tile(instance, tile, centre, entry.render, entry.collision);
        ++publications;
    }
}

bool TerrainBridge::prepare_tile(Instance& instance, Tile& tile, bool incremental) {
    if (!tile.heights.empty()) return true;
    if (!tile.request) {
        if (incremental && pending_loads_.load(std::memory_order_relaxed) >= 4) return false;
        tile.request = std::make_shared<TileLoad>();
        if (incremental) {
            const auto& m = instance.manifest;
            TerrainManifest layout{m.samples, m.spacing_m, m.min_height_m, m.max_height_m, {}};
            pending_loads_.fetch_add(1, std::memory_order_relaxed);
            jobs_.submit(loads_, [this, result = tile.request, layout, source = tile.source] {
                try {
                    read_tile(*result, layout, source);
                } catch (const std::exception& error) {
                    result->error = error.what();
                }
                pending_loads_.fetch_sub(1, std::memory_order_relaxed);
                result->ready.store(true, std::memory_order_release);
            });
            return false;
        }
        read_tile(*tile.request, instance.manifest, tile.source);
        tile.request->ready.store(true, std::memory_order_release);
    }
    if (!tile.request->ready.load(std::memory_order_acquire)) return false;
    tile.heights = std::move(tile.request->heights);
    if (!tile.heights.empty()) return true;
    log_.warn("terrain tile {}: {}", tile.source.file, tile.request->error);
    tile.request.reset();
    tile.failed = true;
    return false;
}

void TerrainBridge::publish_tile(Instance& instance, Tile& tile, Vec3 centre, bool render, bool collision) {
    const auto& manifest = instance.manifest;
    if (collision && !tile.body.valid()) {
        tile.body = physics_.create_body({.shape = ShapeDesc::height_field(tile.heights, manifest.samples, manifest.spacing_m),
                                          .motion = MotionType::Static, .position = centre,
                                          .user_data = to_user_data(instance.owner)});
    }
    if (render && !tile.mesh.valid()) {
        const auto placement = place_tile(manifest, tile.source);
        const auto mesh = tile.request && !tile.request->base_mesh.vertices.empty() ?
            std::move(tile.request->base_mesh) : build_terrain_mesh(manifest, tile.source, tile.heights);
        {
            ASTRAL_PROFILE_SCOPE(&profiler_, "terrain mesh upload");
            tile.mesh = renderer_->create_mesh(mesh);
        }
        if (tile.mesh.valid()) {
            tile.draw = world_.create(TerrainTileDraw{}, Parent{instance.owner}, Transform{.position = placement.corner},
                                      MeshRenderer{tile.mesh, instance.material});
        }
    }
    tile.request.reset();
    if ((collision && !tile.body.valid()) || (render && !tile.mesh.valid())) {
        log_.warn("terrain tile {}: could not create render or collision resources", tile.source.file);
        tile.failed = true;
    }
}

bool TerrainBridge::streaming_enabled() const {
    bool enabled = false;
    world_.each<const TerrainRef>([&](const TerrainRef& ref) { enabled |= streams(ref); });
    return enabled;
}

bool TerrainBridge::character_tiles_ready(Vec3 from, Vec3 to, float radius) const {
    for (const auto& instance : instances_) {
        if (!streams(instance.source) || instance.tiles.empty()) continue;
        const auto* transform = world_.get<const Transform>(instance.owner);
        if (!transform) continue;
        const auto& m = instance.manifest;
        const float half = static_cast<float>(m.samples - 1) * m.spacing_m * 0.5f;
        for (const auto& tile : instance.tiles) {
            if (tile.body.valid()) continue;
            const Vec3 centre = transform->position + tile.offset;
            if (std::min(from.x, to.x) - radius <= centre.x + half &&
                std::max(from.x, to.x) + radius >= centre.x - half &&
                std::min(from.z, to.z) - radius <= centre.z + half &&
                std::max(from.z, to.z) + radius >= centre.z - half) return false;
        }
    }
    return true;
}

void TerrainBridge::sync(std::span<const Vec3> sources, bool finish) {
    if (finish) jobs_.wait(loads_);
    std::erase_if(instances_, [&](Instance& instance) {
        const auto* ref = world_.get<const TerrainRef>(instance.owner);
        const auto* transform = world_.get<const Transform>(instance.owner);
        if (ref && transform && *ref == instance.source && supports_terrain_transform(world_, instance.owner, *transform)) {
            return false;
        }
        release(instance);
        return true;
    });

    world_.each<const TerrainRef, const Transform>([&](Entity owner, const TerrainRef& source, const Transform& transform) {
        if (source.asset.empty() || std::any_of(instances_.begin(), instances_.end(),
            [owner](const Instance& instance) { return instance.owner == owner; })) return;
        if (!supports_terrain_transform(world_, owner, transform) ||
            !std::isfinite(source.render_radius_m) || !std::isfinite(source.collision_radius_m)) {
            log_.warn("terrain {}: expected a root translation, unit scale, and finite streaming radii", source.asset);
            return;
        }
        Instance& instance = instances_.emplace_back(Instance{owner, source, {}, {}, {}});
        load(instance);
    });
    if (instances_.empty()) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "terrain sync");
    std::vector<Vec3> characters;
    if (std::any_of(instances_.begin(), instances_.end(), [](const Instance& i) { return streams(i.source); })) {
        world_.each<const Transform, const CharacterController>([&](const Transform& t, const CharacterController&) {
            characters.push_back(t.position);
        });
    }
    for (auto& instance : instances_) {
        const Vec3 origin = world_.get<const Transform>(instance.owner)->position;
        sync_tiles(instance, origin, sources, characters, finish);
    }
}

void TerrainBridge::update_lod() {
    if (!renderer_ || instances_.empty()) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "terrain LOD");
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
            if (tile.mesh.valid()) addresses.emplace(std::pair{tile.source.x, tile.source.z}, i);
            const Vec3 delta = transform->position + tile.offset - camera;
            const float distance = std::hypot(delta.x, delta.z);
            const float threshold = side * (tile.stride == 1 ? 1.25f : 1.05f);
            choices[i].stride = distance > threshold ? 2u : 1u;
        }
        std::vector<std::pair<std::size_t, MeshHandle>> replacements;
        for (std::size_t i = 0; i < instance.tiles.size(); ++i) {
            auto& choice = choices[i];
            const auto& tile = instance.tiles[i];
            if (!tile.mesh.valid()) continue;
            unsigned neighbor_edges = 0;
            for (unsigned edge = 0; edge < TerrainEdgeCount; ++edge) {
                const auto [dx, dz] = directions[edge];
                const auto neighbor = addresses.find({static_cast<std::int64_t>(tile.source.x) + dx,
                                                      static_cast<std::int64_t>(tile.source.z) + dz});
                if (neighbor == addresses.end()) continue;
                neighbor_edges |= 1u << edge;
                choice.neighbors[edge] = instance.tiles[neighbor->second].heights;
                if (choices[neighbor->second].stride > choice.stride) choice.stitched_edges |= 1u << edge;
            }
            if (choice.stride == tile.stride && choice.stitched_edges == tile.stitched_edges &&
                neighbor_edges == tile.neighbor_edges) continue;
            MeshData data;
            {
                ASTRAL_PROFILE_SCOPE(&profiler_, "terrain LOD mesh build");
                data = build_terrain_mesh(manifest, tile.source, tile.heights, choice);
            }
            MeshHandle mesh;
            {
                ASTRAL_PROFILE_SCOPE(&profiler_, "terrain mesh upload");
                mesh = renderer_->create_mesh(data);
            }
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
            tile.neighbor_edges = 0;
            for (unsigned edge = 0; edge < TerrainEdgeCount; ++edge) {
                if (!choices[i].neighbors[edge].empty()) tile.neighbor_edges |= 1u << edge;
            }
        }
    }
}

} // namespace astral
