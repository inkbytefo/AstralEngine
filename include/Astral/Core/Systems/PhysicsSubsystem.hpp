#pragma once

#include "Astral/Core/ISubsystem.hpp"

namespace Astral {

class PhysicsSubsystem final : public ISubsystem {
public:
    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnShutdown() override;

    [[nodiscard]] SystemStage GetStage() const override { return SystemStage::FixedSimulation; }

    static void Integrate(Registry& registry, float deltaTime);

    static constexpr float DefaultFixedTimeStep = 1.0f / 60.0f;
};

} // namespace Astral
