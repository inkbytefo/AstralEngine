#pragma once

#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Window.hpp"

namespace Astral {

struct FrameContext {
    Registry& registry;
    InputSystem& input;
    Window& window;
    float deltaTime;
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    virtual void OnInit() = 0;
    virtual void OnUpdate(FrameContext& context) = 0;
    virtual void OnShutdown() = 0;

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    bool m_Enabled = true;
};

} // namespace Astral
