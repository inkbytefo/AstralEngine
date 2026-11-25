#pragma once

#include "../string_hash.hpp"

#include <astral/app/vfx.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astral {
class Context;
class TypeRegistry;
class EventRegistry;

enum class VfxTrackKind : std::uint8_t { Smoke, Volume, Particles, Light, Decal, Sound };

struct VfxTrack {
    VfxTrackKind kind;
    float start = 0;
    float duration = 0;
    Vec3 offset{};
    Vec3 color{1, 0.6f, 0.3f};
    float size = 0;
    float strength = 0;
    float soft_distance_m = 0;
    float restitution = 0.45f;
    float lighting = 0;
    std::uint32_t count = 0;
    std::string sound;
    std::string material;
};

struct VfxPlayback {
    std::shared_ptr<const std::vector<VfxTrack>> tracks;
    std::string tag;
    std::size_t next = 0;
    float age = 0;
    float end = 0;
};

struct VfxChild {
    Entity owner;
    float start = 0;
    float end = 0;
};

struct SmokeAppearance {
    MaterialHandle material;
};

struct VfxVolume {
    float cell_size_m;
    float strength;
    float age;
    MaterialHandle material;
    static constexpr std::uint32_t side = 17;
};

struct VfxEmitter {
    ParticleBurst burst;
};

class VfxRuntime {
public:
    Entity play(Context& context, std::string_view asset, Vec3 position);
    void update(Context& context, float delta);
    void apply_smoke_events(World& world);

private:
    struct CachedTemplate {
        std::uint32_t version = 0;
        std::shared_ptr<const std::vector<VfxTrack>> tracks;
        float end = 0;
    };
    struct PendingStart {
        Entity owner;
        Vec3 position;
        VfxTrack track;
        float age = 0;
        std::uint32_t sequence = 0;
    };

    EventReader<PlayVfx> plays_;
    EventReader<StopVfx> stops_;
    EventReader<DisperseSmoke> smoke_events_;
    std::unordered_map<std::string, CachedTemplate, StringHash, std::equal_to<>> template_cache_;
    std::vector<Entity> expired_scratch_;
    std::vector<PendingStart> starts_scratch_;
};

void register_vfx_types(TypeRegistry& types, EventRegistry& events);

} // namespace astral
