#include "terrain_bridge.hpp"

#include "render_bridge.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace astral {
namespace {

bool supports_terrain_transform(const World& world, Entity entity, const Transform& transform) {
    const Vec3 p = transform.position;
    return !world.has<Parent>(entity) && transform.rotation == Quat{} && transform.scale == Vec3{1, 1, 1} &&
           std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

float distance_to_tile(Vec3 point, Vec3 centre, float half) {
    const Vec3 delta = point - centre;
    return std::hypot(std::max(0.0f, std::abs(delta.x) - half), std::max(0.0f, std::abs(delta.z) - half));
}

float nearest_tile_distance(std::span<const Vec3> points, Vec3 centre, float half) {
    float distance = std::numeric_limits<float>::infinity();
    for (const Vec3 point : points) distance = std::min(distance, distance_to_tile(point, centre, half));
    return distance;
}

bool streams(const TerrainRef& ref) noexcept {
    return ref.render_radius_m > 0 || ref.collision_radius_m > 0;
}

} // namespace

TerrainBridge::TerrainBridge(World& world, AssetManager& assets, Logger& log, RenderBridge& render,
                             Renderer* renderer, PhysicsWorld& physics, JobSystem& jobs, Profiler& profiler)
    : world_(world),
      assets_(assets),
      log_(log),
      render_(render),
      renderer_(renderer),
      physics_(physics),
      jobs_(jobs),
      profiler_(profiler) {}

TerrainBridge::~TerrainBridge() {
    jobs_.wait(loads_);
    for (auto& instance : instances_) release(instance);
}

void TerrainBridge::release_tile(Instance& instance, Tile& tile, bool render, bool collision) {
    if (render && tile.mesh.valid()) {
        if (world_.alive(tile.draw)) world_.destroy(tile.draw);
        renderer_->destroy(tile.mesh);
        instance.resident_meshes.erase({tile.source.x, tile.source.z});
        instance.lod_dirty = true;
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
    for (auto& tile : instance.tiles) release_tile(instance, tile, true, true);
    instance.resident_meshes.clear();
    instance.tiles.clear();
    instance.has_lod_state = false;
    instance.lod_dirty = true;
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
    if (renderer_ && !instance.source.material_file.empty()) {
        instance.material = render_.load_material(instance.source.material_file);
    }
    instance.tiles.reserve(instance.manifest.tiles.size());
    for (const auto& source : instance.manifest.tiles) {
        Tile& tile = instance.tiles.emplace_back();
        tile.source = source;
        tile.offset = place_tile(instance.manifest, source).centre;
    }
    instance.lod_dirty = true;
}

void TerrainBridge::read_tile(TileLoad& result, const TerrainManifest& manifest, const TerrainTileSource& source) const {
    const auto bytes = assets_.read(source.file, &result.error);
    const auto samples = bytes ? read_terrain_tile(*bytes, manifest.samples, &result.error) : std::nullopt;
    if (!samples) return;
    result.heights.reserve(samples->size());
    for (const auto sample : *samples) result.heights.push_back(manifest.height_m(sample));
    if (renderer_) result.base_mesh = build_terrain_mesh(manifest, source, result.heights);
}

void TerrainBridge::sync_tiles(Instance& instance, Vec3 origin, std::span<const Vec3> sources,
                               std::span<const Vec3> characters, bool finish) {
    if (instance.tiles.empty()) return;
    const auto& manifest = instance.manifest;
    const float half = static_cast<float>(manifest.samples - 1) * manifest.spacing_m * 0.5f;
    const float collision_radius = instance.source.collision_radius_m > 0
                                       ? std::max(instance.source.collision_radius_m, instance.source.render_radius_m)
                                       : 0.0f;
    const bool incremental = streams(instance.source) && !finish;

    wanted_scratch_.clear();
    for (std::size_t i = 0; i < instance.tiles.size(); ++i) {
        auto& tile = instance.tiles[i];
        const Vec3 centre = origin + tile.offset;
        if (!std::isfinite(centre.x) || !std::isfinite(centre.z)) continue;
        const float distance = nearest_tile_distance(sources, centre, half);
        const float character_distance = nearest_tile_distance(characters, centre, half);
        const bool render = renderer_ && (instance.source.render_radius_m <= 0 ||
                                          distance <= instance.source.render_radius_m + (tile.mesh.valid() ? half * 0.5f : 0.0f));
        const bool collision = collision_radius <= 0 ||
                               std::min(distance, character_distance) <=
                                   collision_radius + (tile.body.valid() ? half * 0.5f : 0.0f);
        release_tile(instance, tile, !render, !collision);
        if (!render && !collision) {
            tile.request.reset();
            std::vector<float>().swap(tile.heights);
            tile.pending_mesh = {};
            tile.failed = false;
            continue;
        }
        wanted_scratch_.push_back({i, std::min(distance, character_distance), render, collision});
    }
    std::stable_sort(wanted_scratch_.begin(), wanted_scratch_.end(),
                     [](const WantedTile& a, const WantedTile& b) { return a.distance < b.distance; });

    unsigned publications = 0;
    for (const auto& entry : wanted_scratch_) {
        auto& tile = instance.tiles[entry.index];
        const Vec3 centre = origin + tile.offset;
        if (tile.body.valid() && centre != physics_.position(tile.body)) physics_.set_transform(tile.body, centre, {});
        if (tile.failed || ((!entry.render || tile.mesh.valid()) && (!entry.collision || tile.body.valid()))) continue;
        if (incremental && publications >= 4) continue;
        if (!prepare_tile(instance, tile, incremental)) continue;
        publish_tile(instance, entry.index, centre, entry.render, entry.collision);
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
    tile.pending_mesh = std::move(tile.request->base_mesh);
    const std::string err = std::move(tile.request->error);
    tile.request.reset();
    if (!tile.heights.empty()) return true;
    log_.warn("terrain tile {}: {}", tile.source.file, err);
    tile.failed = true;
    return false;
}

void TerrainBridge::publish_tile(Instance& instance, std::size_t tile_idx, Vec3 centre, bool render, bool collision) {
    Tile& tile = instance.tiles[tile_idx];
    const auto& manifest = instance.manifest;
    if (collision && !tile.body.valid()) {
        tile.body = physics_.create_body({.shape = ShapeDesc::height_field(tile.heights, manifest.samples, manifest.spacing_m),
                                          .motion = MotionType::Static,
                                          .position = centre,
                                          .user_data = to_user_data(instance.owner)});
    }
    if (render && !tile.mesh.valid()) {
        const auto placement = place_tile(manifest, tile.source);
        const MeshData mesh = !tile.pending_mesh.vertices.empty()
                                  ? std::move(tile.pending_mesh)
                                  : build_terrain_mesh(manifest, tile.source, tile.heights);
        tile.pending_mesh = {};
        {
            ASTRAL_PROFILE_SCOPE(&profiler_, "terrain mesh upload");
            tile.mesh = renderer_->create_mesh(mesh);
        }
        if (tile.mesh.valid()) {
            tile.draw = world_.create(TerrainTileDraw{}, Parent{instance.owner}, Transform{.position = placement.corner},
                                      MeshRenderer{tile.mesh, instance.material});
            instance.resident_meshes[{tile.source.x, tile.source.z}] = tile_idx;
            instance.lod_dirty = true;
        }
    }
    if ((collision && !tile.body.valid()) || (render && !tile.mesh.valid())) {
        log_.warn("terrain tile {}: could not create render or collision resources", tile.source.file);
        tile.failed = true;
    }
}

bool TerrainBridge::streaming_enabled() const {
    if (world_.count<TerrainRef>() == 0) return false;
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
                std::max(from.z, to.z) + radius >= centre.z - half) {
                return false;
            }
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
        if (source.asset.empty() ||
            std::any_of(instances_.begin(), instances_.end(), [owner](const Instance& i) { return i.owner == owner; })) {
            return;
        }
        if (!supports_terrain_transform(world_, owner, transform) || !std::isfinite(source.render_radius_m) ||
            !std::isfinite(source.collision_radius_m)) {
            log_.warn("terrain {}: expected a root translation, unit scale, and finite streaming radii", source.asset);
            return;
        }
        Instance& instance = instances_.emplace_back(Instance{owner, source, {}, {}, {}, {}, {}, {}, false, true});
        load(instance);
    });
    if (instances_.empty()) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "terrain sync");
    characters_scratch_.clear();
    if (std::any_of(instances_.begin(), instances_.end(), [](const Instance& i) { return streams(i.source); })) {
        world_.each<const Transform, const CharacterController>(
            [&](const Transform& t, const CharacterController&) { characters_scratch_.push_back(t.position); });
    }
    for (auto& instance : instances_) {
        const Vec3 origin = world_.get<const Transform>(instance.owner)->position;
        sync_tiles(instance, origin, sources, characters_scratch_, finish);
    }
}

void TerrainBridge::commit_lod_replacements(Instance& instance) {
    // Commit the whole transition together so a failed upload cannot leave a mismatched edge.
    for (const auto& [i, mesh] : lod_replacements_) {
        auto& tile = instance.tiles[i];
        auto* draw = world_.get<MeshRenderer>(tile.draw);
        if (!draw) {
            renderer_->destroy(mesh);
            continue;
        }
        draw->mesh = mesh;
        renderer_->destroy(tile.mesh);
        tile.mesh = mesh;
        tile.stride = lod_choices_[i].stride;
        tile.stitched_edges = lod_choices_[i].stitched_edges;
        tile.neighbor_edges = 0;
        for (unsigned edge = 0; edge < TerrainEdgeCount; ++edge) {
            if (!lod_choices_[i].neighbors[edge].empty()) tile.neighbor_edges |= 1u << edge;
        }
    }
}

void TerrainBridge::update_instance_lod(Instance& instance, const Vec3& camera) {
    if (instance.tiles.empty() || instance.resident_meshes.empty()) return;
    const auto* transform = world_.get<const Transform>(instance.owner);
    if (!transform) return;
    if (instance.has_lod_state && !instance.lod_dirty && instance.last_lod_camera == camera &&
        instance.last_lod_origin == transform->position) {
        return;
    }

    constexpr std::array<std::pair<int, int>, TerrainEdgeCount> directions{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
    const auto& manifest = instance.manifest;
    const float side = static_cast<float>(manifest.samples - 1) * manifest.spacing_m;
    lod_choices_.assign(instance.tiles.size(), {});
    for (const auto& [coord, i] : instance.resident_meshes) {
        const auto& tile = instance.tiles[i];
        const Vec3 delta = transform->position + tile.offset - camera;
        const float distance = std::hypot(delta.x, delta.z);
        const float threshold = side * (tile.stride == 1 ? 1.25f : 1.05f);
        lod_choices_[i].stride = distance > threshold ? 2u : 1u;
    }

    lod_replacements_.clear();
    bool upload_failed = false;
    for (const auto& [coord, i] : instance.resident_meshes) {
        auto& choice = lod_choices_[i];
        const auto& tile = instance.tiles[i];
        unsigned neighbor_edges = 0;
        for (unsigned edge = 0; edge < TerrainEdgeCount; ++edge) {
            const auto [dx, dz] = directions[edge];
            const auto neighbor = instance.resident_meshes.find({coord.x + dx, coord.z + dz});
            if (neighbor == instance.resident_meshes.end()) continue;
            neighbor_edges |= 1u << edge;
            choice.neighbors[edge] = instance.tiles[neighbor->second].heights;
            if (lod_choices_[neighbor->second].stride > choice.stride) choice.stitched_edges |= 1u << edge;
        }
        if (choice.stride == tile.stride && choice.stitched_edges == tile.stitched_edges &&
            neighbor_edges == tile.neighbor_edges) {
            continue;
        }
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
            for (const auto& replacement : lod_replacements_) renderer_->destroy(replacement.second);
            lod_replacements_.clear();
            upload_failed = true;
            break;
        }
        lod_replacements_.emplace_back(i, mesh);
    }

    commit_lod_replacements(instance);
    if (!upload_failed) {
        instance.last_lod_camera = camera;
        instance.last_lod_origin = transform->position;
        instance.has_lod_state = true;
        instance.lod_dirty = false;
    }
}

void TerrainBridge::update_lod() {
    if (!renderer_ || instances_.empty()) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "terrain LOD");
    const Vec3 camera = render_.camera_position();
    for (auto& instance : instances_) update_instance_lod(instance, camera);
}

} // namespace astral
