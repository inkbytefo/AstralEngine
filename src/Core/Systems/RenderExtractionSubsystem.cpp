#include "Astral/Core/Systems/RenderExtractionSubsystem.hpp"

#include "Astral/Core/RenderExtractionSystem.hpp"

namespace Astral {

void RenderExtractionSubsystem::OnInit() {}

void RenderExtractionSubsystem::OnUpdate(FrameContext& context) {
    ExtractRenderData(context.registry, m_SceneEdits, m_SceneEntities);
}

void RenderExtractionSubsystem::OnShutdown() {}

} // namespace Astral
