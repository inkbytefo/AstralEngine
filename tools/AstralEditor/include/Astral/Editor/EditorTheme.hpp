#pragma once
#include <imgui.h>

namespace Astral {

// Shared opaque surfaces and semantic accents for native editor widgets.
namespace EditorPalette {
inline const ImVec4 Canvas{0.090f, 0.098f, 0.110f, 1.0f};
inline const ImVec4 Panel{0.125f, 0.137f, 0.153f, 1.0f};
inline const ImVec4 Raised{0.157f, 0.169f, 0.188f, 1.0f};
inline const ImVec4 Input{0.098f, 0.110f, 0.125f, 1.0f};
inline const ImVec4 Hover{0.200f, 0.220f, 0.247f, 1.0f};
inline const ImVec4 Border{0.235f, 0.255f, 0.282f, 1.0f};
inline const ImVec4 Text{0.875f, 0.894f, 0.918f, 1.0f};
inline const ImVec4 Muted{0.580f, 0.627f, 0.686f, 1.0f};
inline const ImVec4 Accent{0.176f, 0.365f, 0.667f, 1.0f};
inline const ImVec4 AccentHover{0.220f, 0.420f, 0.750f, 1.0f};
inline const ImVec4 Selection{0.157f, 0.235f, 0.353f, 1.0f};
inline const ImVec4 AxisX{0.86f, 0.40f, 0.43f, 1.0f};
inline const ImVec4 AxisY{0.46f, 0.73f, 0.52f, 1.0f};
inline const ImVec4 AxisZ{0.39f, 0.62f, 0.90f, 1.0f};
}

/// Apply the matte dark editor theme with Astral blue accents.
void ApplyAstralTheme(bool isPlayMode = false);
/// Load Segoe UI on Windows, otherwise the ImGui default font.
void InitEditorFonts(ImGuiIO& io);

} // namespace Astral
