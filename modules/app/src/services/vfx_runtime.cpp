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

enum class TrackKind { Smoke, Particles, Light, Decal, Sound };
struct Track {
    TrackKind kind;
    float start, duration;
    Vec3 offset{}, color{1, 0.6f, 0.3f};
    float size, strength;
    std::uint32_t count;
    std::string sound, material;
};
struct Playback {
    std::vector<Track> tracks;
    std::string tag;
    std::size_t next = 0;
    float age = 0, end = 0;
};

std::optional<TrackKind> track_kind(std::string_view name) {
    if (name == "smoke") return TrackKind::Smoke;
    if (name == "particles") return TrackKind::Particles;
    if (name == "light") return TrackKind::Light;
    if (name == "decal") return TrackKind::Decal;
    if (name == "sound") return TrackKind::Sound;
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
        const double n = component.as_number();
        if (!component.is_number() || !std::isfinite(n) || n < low || n > high) return false;
        components[i] = static_cast<float>(n);
    }
    out = {components[0], components[1], components[2]};
    return true;
}

std::optional<Playback> parse_vfx(const Json& json) {
    const auto& tracks = json["tracks"];
    if (json["format"].as_string() != "astral.vfx" || !json["version"].is_number() ||
        json["version"].as_number() != 1 || !tracks.is_array() || tracks.size() == 0 || tracks.size() > 32) return {};
    Playback result;
    for (const auto& node : tracks.as_array()) {
        const auto kind = track_kind(node["type"].as_string());
        if (!kind) return {};
        Track t{.kind = *kind, .start = 0, .duration = 0, .size = 0, .strength = 0, .count = 0, .sound = {}, .material = {}};
        float count;
        if (!read_scalar(node, "start", t.start, 0, 0, 60) ||
            !read_scalar(node, "duration", t.duration, 1, 0.05, 60) ||
            !read_scalar(node, "size", t.size, *kind == TrackKind::Particles ? 0.05 : 2.0, 0.01, 100) ||
            !read_scalar(node, "strength", t.strength, 1, 0, 100) ||
            !read_scalar(node, "count", count, 64, 1, 1024) || count != std::floor(count) ||
            !read_xyz(node, "offset", t.offset, -1000, 1000) || !read_xyz(node, "color", t.color, 0, 100)) return {};
        t.count = static_cast<std::uint32_t>(count);
        if (const auto* material = node.find("material")) {
            if (!material->is_string()) return {};
            t.material = material->as_string();
        }
        if (*kind == TrackKind::Sound) {
            t.sound = node["asset"].as_string();
            if (t.sound.empty()) return {};
        }
        result.end = std::max(result.end, t.start + t.duration);
        result.tracks.push_back(std::move(t));
    }
    std::stable_sort(result.tracks.begin(), result.tracks.end(), [](const Track& a, const Track& b) { return a.start < b.start; });
    return result;
}

} // namespace

struct VfxChild {
    Entity owner;
    float start, end;
};

namespace {
void spawn_track(Context& ctx, Entity owner, Vec3 position, const Track& t, float age, std::uint32_t sequence) {
    const VfxChild child{owner, t.start, t.start + t.duration};
    Transform transform{.position = position + t.offset};
    const auto material = t.material.empty() ? MaterialHandle{} : ctx.materials().load_material(t.material);
    switch (t.kind) {
    case TrackKind::Smoke:
        ctx.world.create(transform, child, SmokeAppearance{material}, GameplaySmoke({
            .cell_size_m = t.size / 17.0f, .extinction_per_m = t.strength, .spread_interval_steps = 1,
            .spread_steps = static_cast<unsigned>(t.duration * 0.75f / ctx.time.fixed_delta),
            .fade_steps = static_cast<unsigned>(t.duration * 0.25f / ctx.time.fixed_delta)}));
        break;
    case TrackKind::Particles:
        ctx.world.create(transform, child, VfxEmitter{{
            .age = age - t.start, .drift = ctx.fields.wind_at(transform.position, ctx.time.elapsed),
            .duration = t.duration, .color = t.color * t.strength, .size_m = t.size,
            .count = t.count, .seed = owner.index * 37u + sequence, .material = material}});
        break;
    case TrackKind::Light:
        ctx.world.create(transform, child, PointLight{.color = t.color, .intensity = t.strength, .radius = t.size});
        break;
    case TrackKind::Decal:
        transform.scale = {t.size, t.size * 0.25f, t.size};
        ctx.world.create(transform, child, Decal{.color = {t.color.x, t.color.y, t.color.z, std::min(1.0f, t.strength)}});
        break;
    case TrackKind::Sound:
        ctx.world.create(transform, child, AudioSource{.sound = t.sound, .volume = std::min(1.0f, t.strength), .loop = false});
        break;
    }
}
} // namespace

Entity play_vfx(Context& ctx, std::string_view asset, Vec3 position) {
    if (ctx.world.count<Playback>() >= 64 || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z)) return {};
    const auto id = ctx.assets->load_document(asset);
    const auto* doc = ctx.assets->document(id);
    auto playback = doc ? parse_vfx(doc->json) : std::nullopt;
    if (!playback) {
        ctx.log.warn("vfx: could not load a valid v1 timeline from {}", asset);
        return {};
    }
    return ctx.world.create(Transform{.position = position}, std::move(*playback));
}

void stop_vfx(Context& ctx, Entity instance) {
    if (ctx.world.has<Playback>(instance)) ctx.world.destroy(instance);
}

void register_vfx_types(TypeRegistry& types, EventRegistry& events) {
    events.add<PlayVfx>();
    events.add<StopVfx>();
    types.exclude<Playback>();
    types.exclude<VfxChild>();
}

void VfxRuntime::update(Context& ctx, float delta) {
    World& world = ctx.world;
    plays_.read(world, [&](const PlayVfx& play) {
        const auto entity = play_vfx(ctx, play.asset, play.position);
        if (auto* playback = world.get<Playback>(entity)) playback->tag = play.tag;
    });
    std::vector<Entity> expired;
    stops_.read(world, [&](const StopVfx& stop) {
        if (stop.tag.empty()) return;
        world.each<const Playback>([&](Entity e, const Playback& playback) {
            if (playback.tag == stop.tag) expired.push_back(e);
        });
    });
    for (const Entity e : expired) {
        if (world.alive(e)) world.destroy(e);
    }
    expired.clear();
    struct Start { Entity owner; Vec3 position; Track track; float age; std::uint32_t sequence; };
    std::vector<Start> starts;
    world.each<Playback, const Transform>([&](Entity e, Playback& playback, const Transform& transform) {
        const float age = playback.age;
        while (playback.next < playback.tracks.size() && playback.tracks[playback.next].start <= age) {
            const auto i = playback.next++;
            const auto& t = playback.tracks[i];
            if (age < t.start + t.duration) starts.push_back({e, transform.position, t, age, static_cast<std::uint32_t>(i)});
        }
        if (age >= playback.end) expired.push_back(e);
        playback.age += delta;
    });
    world.each<const VfxChild>([&](Entity e, const VfxChild& child) {
        const auto* playback = world.get<const Playback>(child.owner);
        if (!playback || playback->age - delta >= child.end) {
            expired.push_back(e);
        } else if (auto* emitter = world.get<VfxEmitter>(e)) {
            emitter->burst.age = playback->age - delta - child.start;
        }
    });
    for (const auto& start : starts) {
        spawn_track(ctx, start.owner, start.position, start.track, start.age, start.sequence);
    }
    for (const Entity e : expired) world.destroy(e);
}

} // namespace astral
