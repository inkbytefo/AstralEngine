#pragma once

#include <astral/render/renderer.hpp>
#include <astral/app/vfx.hpp>

namespace astral {
struct Context;
class TypeRegistry;
class EventRegistry;
struct VfxChild;
struct VfxEmitter {
    ParticleBurst burst;
};
class VfxRuntime {
public:
    void update(Context& context, float delta);

private:
    EventReader<PlayVfx> plays_;
    EventReader<StopVfx> stops_;
};
void register_vfx_types(TypeRegistry& types, EventRegistry& events);
} // namespace astral
