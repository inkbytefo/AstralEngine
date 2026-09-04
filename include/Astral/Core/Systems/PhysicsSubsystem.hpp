#pragma once

#include "Astral/Core/ISubsystem.hpp"

namespace Astral {

class PhysicsSubsystem final : public ISubsystem {
public:
    void OnInit() override;
    void OnUpdate(FrameContext& context) override;
    void OnShutdown() override;

    static void Integrate(Registry& registry, float deltaTime);

private:
    static constexpr float FixedTimeStep = 0.016f;
};

} // namespace Astral
