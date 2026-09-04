#pragma once

#include <imgui.h>
#include <cstddef>

namespace Astral {

struct StatusBarInfo {
    float gpuTimeMs     = 0.0f;
    float cpuTimeMs     = 0.0f;
    size_t entityCount  = 0;
    bool gridEnabled    = true;
    bool taaEnabled     = true;
};

/// Draws the fixed-height status bar at the bottom of the main viewport.
void DrawEditorStatusBar(const StatusBarInfo& info);

} // namespace Astral
