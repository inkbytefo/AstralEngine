#include "Astral/Core/Systems/TransformSubsystem.hpp"

#include "Astral/Core/TransformSystem.hpp"

namespace Astral {

void TransformSubsystem::OnInit() {}

void TransformSubsystem::OnUpdate(FrameContext& context) {
    UpdateWorldTransforms(context.registry);
}

void TransformSubsystem::OnShutdown() {}

} // namespace Astral
