#include "Astral/Core/Systems/InputSubsystem.hpp"

namespace Astral {

void InputSubsystem::OnInit() {}

void InputSubsystem::OnUpdate(FrameContext& context) {
    context.input.BeginFrame();
    context.actions.Update(context.input);
}

void InputSubsystem::OnShutdown() {}

} // namespace Astral
