#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"

#include "Astral/Core/RenderExtractionSystem.hpp"

namespace Astral {

void RenderExtractionSubsystem::OnInit() {}

void RenderExtractionSubsystem::OnUpdate(FrameContext& context) {
    m_Snapshot = SDFSceneSnapshot::Extract(context.registry, 1);
}

void RenderExtractionSubsystem::OnShutdown() {}

} // namespace Astral
