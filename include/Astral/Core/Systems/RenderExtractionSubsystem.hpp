#pragma once

#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/EntityHandle.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"

#include <vector>

namespace Astral {

class RenderExtractionSubsystem final : public ISubsystem {
public:
    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] SystemStage GetStage() const override { return SystemStage::RenderExtraction; }

    [[nodiscard]] const SDFSceneSnapshot& GetLastExtractedSnapshot() const noexcept {
        return m_Snapshot;
    }

    [[nodiscard]] const std::vector<SDFPrimitiveRecord>& GetLastExtractedRecords() const noexcept {
        return m_Snapshot.GetRecords();
    }

    [[nodiscard]] const std::vector<EntityHandle>& GetLastExtractedEntities() const noexcept {
        return m_Snapshot.GetEntities();
    }

private:
    SDFSceneSnapshot m_Snapshot;
};

} // namespace Astral
