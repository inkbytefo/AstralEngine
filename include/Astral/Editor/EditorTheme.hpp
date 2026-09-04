#pragma once

#include <imgui.h>

namespace Astral {

/// Photoshop 2026 Dark Theme — Astral Engine Edition
/// Sampled from the ImageEditor example with engine-specific accent colors.
void ApplyAstralTheme();

/// Load Segoe UI font on Windows, fallback to ImGui default on other platforms.
void InitEditorFonts(ImGuiIO& io);

} // namespace Astral
