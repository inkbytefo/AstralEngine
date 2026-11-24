#pragma once

#include <astral/render/renderer.hpp>
#include <astral/app/vfx.hpp>

namespace astral {
class Context;
class TypeRegistry;
class EventRegistry;
struct VfxChild;
struct SmokeAppearance { MaterialHandle material; };
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
    void update(Context& context, float delta);
    void apply_smoke_events(World& world);

private:
    EventReader<PlayVfx> plays_;
    EventReader<StopVfx> stops_;
    EventReader<DisperseSmoke> smoke_events_;
};
void register_vfx_types(TypeRegistry& types, EventRegistry& events);
} // namespace astral
