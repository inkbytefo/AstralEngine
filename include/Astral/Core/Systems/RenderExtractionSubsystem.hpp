#pragma once

#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/EntityHandle.hpp"
#include "Astral/Renderer/SDFEdit.hpp"

#include <vector>

namespace Astral {

class RenderExtractionSubsystem final : public ISubsystem {
public:
    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] const std::vector<SDFEditGPU>& GetLastExtractedEdits() const noexcept {
        return m_SceneEdits;
    }

    [[nodiscard]] const std::vector<EntityHandle>& GetLastExtractedEntities() const noexcept {
        return m_SceneEntities;
    }

private:
    std::vector<SDFEditGPU> m_SceneEdits;
    std::vector<EntityHandle> m_SceneEntities;
};

} // namespace Astral
