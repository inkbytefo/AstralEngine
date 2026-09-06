#pragma once

#include <imgui.h>

namespace Astral {

/// Programmatically build the default docking layout using ImGui DockBuilder API.
/// @param dockspace_id  The DockSpace node ID to configure.
/// @param force         If true, reset the layout even if one already exists (e.g. from imgui.ini).
void SetupDefaultEditorLayout(ImGuiID dockspace_id, bool force);

inline constexpr const char* EditorToolsWindow = "Araclar###AstralEditorTools";

/// Shared by the workspace reservation and the fixed status bar.
inline float EditorStatusBarHeight() { return ImGui::GetTextLineHeight() + 12.0f; }

} // namespace Astral
