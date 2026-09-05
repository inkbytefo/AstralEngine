#pragma once

#include "Astral/Core/ISubsystem.hpp"

namespace Astral {

class TransformSubsystem final : public ISubsystem {
public:
    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] SystemStage GetStage() const override { return SystemStage::Transform; }
};

} // namespace Astral
