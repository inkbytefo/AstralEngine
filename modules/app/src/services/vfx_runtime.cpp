#include "vfx_runtime.hpp"

#include <astral/app/app.hpp>
#include <astral/app/gameplay_smoke.hpp>
#include <astral/app/vfx.hpp>
#include <astral/assets/asset_manager.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace astral {
namespace {

std::optional<VfxTrackKind> track_kind(std::string_view name) {
    if (name == "smoke") return VfxTrackKind::Smoke;
    if (name == "volume") return VfxTrackKind::Volume;
    if (name == "particles") return VfxTrackKind::Particles;
    if (name == "light") return VfxTrackKind::Light;
    if (name == "decal") return VfxTrackKind::Decal;
    if (name == "sound") return VfxTrackKind::Sound;
    return std::nullopt;
}

bool read_scalar(const Json& obj, const char* key, float& out, double fallback, double low, double high) {
    const auto* value = obj.find(key);
    if (value && !value->is_number()) return false;
    const double n = value ? value->as_number() : fallback;
    if (!std::isfinite(n) || n < low || n > high) return false;
    out = static_cast<float>(n);
    return true;
}

bool read_xyz(const Json& obj, const char* key, Vec3& out, double low, double high) {
    const auto* value = obj.find(key);
    if (!value) return true;
    if (!value->is_array() || value->size() != 3) return false;
    float components[3];
    for (unsigned i = 0; i < 3; ++i) {
        const auto& component = (*value)[static_cast<std::size_t>(i)];
        if (!component.is_number()) return false;
        const double n = component.as_number();
        if (!std::isfinite(n) || n < low || n > high) return false;
        components[i] = static_cast<float>(n);
    }
    out = {components[0], components[1], components[2]};
    return true;
}

struct ParsedVfx {
    std::vector<VfxTrack> tracks;
    float end = 0;
};

std::optional<ParsedVfx> parse_vfx(const Json& json) {
    if (!json.is_object()) return {};
    const auto& tracks = json["tracks"];
    if (json["format"].as_string() != "astral.vfx" || !json["version"].is_number() ||
        json["version"].as_number() != 1 || !tracks.is_array() || tracks.size() == 0 || tracks.size() > 32) {
        return {};
    }
    ParsedVfx result;
    for (const auto& node : tracks.as_array()) {
        if (!node.is_object()) return {};
        const auto kind = track_kind(node["type"].as_string());
        if (!kind) return {};
        VfxTrack t{.kind = *kind};
        float count = 0;
        if (!read_scalar(node, "start", t.start, 0, 0, 60) ||
            !read_scalar(node, "duration", t.duration, 1, 0.05, 60) ||
            !read_scalar(node, "size", t.size, *kind == VfxTrackKind::Particles ? 0.05 : 2.0, 0.01, 100) ||
            !read_scalar(node, "strength", t.strength, 1, 0, 100) ||
            (*kind == VfxTrackKind::Particles && !read_scalar(node, "soft_distance_m", t.soft_distance_m, 0, 0, 20)) ||
            (*kind == VfxTrackKind::Particles && !read_scalar(node, "restitution", t.restitution, 0.45, 0, 1)) ||
            (*kind == VfxTrackKind::Particles && !read_scalar(node, "lighting", t.lighting, 0, 0, 1)) ||
            !read_scalar(node, "count", count, 64, 1, 1024) || count != std::floor(count) ||
            !read_xyz(node, "offset", t.offset, -1000, 1000) || !read_xyz(node, "color", t.color, 0, 100)) {
            return {};
        }
        t.count = static_cast<std::uint32_t>(count);
        if (const auto* material = node.find("material")) {
            if (!material->is_string()) return {};
            t.material = material->as_string();
        }
        if (*kind == VfxTrackKind::Volume && t.material.empty()) return {};
        if (*kind == VfxTrackKind::Sound) {
            t.sound = node["asset"].as_string();
            if (t.sound.empty()) return {};
        }
        result.end = std::max(result.end, t.start + t.duration);
        result.tracks.push_back(std::move(t));
    }
    std::stable_sort(result.tracks.begin(), result.tracks.end(),
                     [](const VfxTrack& a, const VfxTrack& b) { return a.start < b.start; });
    return result;
}

void spawn_track(Context& ctx, Entity owner, Vec3 position, const VfxTrack& t, float age, std::uint32_t sequence) {
    const VfxChild child{owner, t.start, t.start + t.duration};
    Transform transform{.position = position + t.offset};
    const auto material = t.material.empty() ? MaterialHandle{} : ctx.materials().load_material(t.material);
    switch (t.kind) {
    case VfxTrackKind::Smoke:
        ctx.world.create(transform, child, SmokeAppearance{material},
                         GameplaySmoke({.cell_size_m = t.size / 17.0f,
                                        .extinction_per_m = t.strength,
                                        .spread_interval_steps = 1,
                                        .spread_steps = static_cast<unsigned>(t.duration * 0.75f / ctx.time.fixed_delta),
                                        .fade_steps = static_cast<unsigned>(t.duration * 0.25f / ctx.time.fixed_delta)}));
        break;
    case VfxTrackKind::Volume:
        ctx.world.create(transform, child, VfxVolume{t.size / VfxVolume::side, t.strength, age - t.start, material});
        break;
    case VfxTrackKind::Particles:
        ctx.world.create(transform, child,
                         VfxEmitter{{.age = age - t.start,
                                     .drift = ctx.fields.wind_at(transform.position, ctx.time.elapsed),
                                     .duration = t.duration,
                                     .color = t.color * t.strength,
                                     .size_m = t.size,
                                     .count = t.count,
                                     .seed = owner.index * 37u + sequence,
                                     .material = material,
                                     .soft_distance_m = t.soft_distance_m,
                                     .restitution = t.restitution,
                                     .lighting = t.lighting}});
        break;
    case VfxTrackKind::Light:
        ctx.world.create(transform, child, PointLight{.color = t.color, .intensity = t.strength, .radius = t.size});
        break;
    case VfxTrackKind::Decal:
        transform.scale = {t.size, t.size * 0.25f, t.size};
        ctx.world.create(transform, child, Decal{.color = {t.color.x, t.color.y, t.color.z, std::min(1.0f, t.strength)}});
        break;
    case VfxTrackKind::Sound:
        ctx.world.create(transform, child, AudioSource{.sound = t.sound, .volume = std::min(1.0f, t.strength), .loop = false});
        break;
    }
}

} // namespace

Entity VfxRuntime::play(Context& ctx, std::string_view asset, Vec3 position) {
    if (ctx.world.count<VfxPlayback>() >= 64 || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z)) {
        return {};
    }
    const auto id = ctx.assets->load_document(asset);
    const auto* doc = ctx.assets->document(id);
    if (!doc) {
        ctx.log.warn("vfx: could not load a valid v1 timeline from {}", asset);
        return {};
    }
    const auto it = template_cache_.find(asset);
    if (it != template_cache_.end() && it->second.version == doc->version) {
        return ctx.world.create(Transform{.position = position},
                                VfxPlayback{.tracks = it->second.tracks, .end = it->second.end});
    }
    auto parsed = parse_vfx(doc->json);
    if (!parsed) {
        if (it != template_cache_.end()) template_cache_.erase(it);
        ctx.log.warn("vfx: could not load a valid v1 timeline from {}", asset);
        return {};
    }
    auto shared_tracks = std::make_shared<const std::vector<VfxTrack>>(std::move(parsed->tracks));
    const float end = parsed->end;
    if (it != template_cache_.end()) {
        it->second = CachedTemplate{doc->version, shared_tracks, end};
    } else {
        template_cache_.emplace(std::string(asset), CachedTemplate{doc->version, shared_tracks, end});
    }
    return ctx.world.create(Transform{.position = position}, VfxPlayback{.tracks = std::move(shared_tracks), .end = end});
}

void stop_vfx(Context& ctx, Entity instance) {
    if (ctx.world.has<VfxPlayback>(instance)) ctx.world.destroy(instance);
}

void register_vfx_types(TypeRegistry& types, EventRegistry& events) {
    events.add<PlayVfx>();
    events.add<StopVfx>();
    events.add<DisperseSmoke>();
    types.exclude<VfxPlayback>();
    types.exclude<VfxChild>();
    types.exclude<VfxVolume>();
    types.exclude<VfxEmitter>();
    types.exclude<SmokeAppearance>();
}

void VfxRuntime::update(Context& ctx, float delta) {
    World& world = ctx.world;
    plays_.read(world, [&](const PlayVfx& play_evt) {
        const auto entity = play(ctx, play_evt.asset, play_evt.position);
        if (auto* playback = world.get<VfxPlayback>(entity)) playback->tag = play_evt.tag;
    });
    expired_scratch_.clear();
    stops_.read(world, [&](const StopVfx& stop) {
        if (stop.tag.empty()) return;
        world.each<const VfxPlayback>([&](Entity e, const VfxPlayback& playback) {
            if (playback.tag == stop.tag) expired_scratch_.push_back(e);
        });
    });
    for (const Entity e : expired_scratch_) {
        if (world.alive(e)) world.destroy(e);
    }
    expired_scratch_.clear();
    starts_scratch_.clear();
    world.each<VfxPlayback, const Transform>([&](Entity e, VfxPlayback& playback, const Transform& transform) {
        const float age = playback.age;
        if (playback.tracks) {
            const auto& tracks = *playback.tracks;
            while (playback.next < tracks.size() && tracks[playback.next].start <= age) {
                const auto i = playback.next++;
                const auto& t = tracks[i];
                if (age < t.start + t.duration) {
                    starts_scratch_.push_back({e, transform.position, t, age, static_cast<std::uint32_t>(i)});
                }
            }
        }
        if (age >= playback.end) expired_scratch_.push_back(e);
        playback.age += delta;
    });
    world.each<const VfxChild>([&](Entity e, const VfxChild& child) {
        const auto* playback = world.get<const VfxPlayback>(child.owner);
        if (!playback || playback->age - delta >= child.end) {
            expired_scratch_.push_back(e);
        } else if (auto* emitter = world.get<VfxEmitter>(e)) {
            emitter->burst.age = playback->age - delta - child.start;
            if (const auto* transform = world.get<const Transform>(e)) {
                emitter->burst.drift = ctx.fields.wind_at(transform->position, ctx.time.elapsed);
            }
        } else if (auto* volume = world.get<VfxVolume>(e)) {
            volume->age = playback->age - delta - child.start;
        }
    });
    for (const auto& start : starts_scratch_) {
        spawn_track(ctx, start.owner, start.position, start.track, start.age, start.sequence);
    }
    for (const Entity e : expired_scratch_) {
        if (world.alive(e)) world.destroy(e);
    }
}

void VfxRuntime::apply_smoke_events(World& world) {
    smoke_events_.read(world, [&](const DisperseSmoke& event) {
        world.each<GameplaySmoke, const Transform>([&](Entity entity, GameplaySmoke& smoke, const Transform& transform) {
            if (!world.has<Parent>(entity)) {
                smoke.disperse(transform.position, event.from, event.to, event.radius_m, event.hold_steps);
            }
        });
    });
}

} // namespace astral
