#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"

namespace Astral {

class InputSystem;

/// Draws the main editor menu bar inside the DockSpace host window.
/// Returns actions via out-parameters so the caller (EditorUI) can execute them.
struct MenuBarActions {
    bool resetLayout   = false;
    bool showDemoWindow = false;
    bool addSphere     = false;
    bool addBox        = false;
    bool addTorus      = false;
    bool addCylinder   = false;
    bool addPlane      = false;
    bool deleteSelected = false;
    bool clearScene    = false;
    bool exitApp       = false;
    bool newScene      = false;
    bool saveScene     = false;
    bool openScene     = false;
};

void DrawEditorMenuBar(Scene& scene, Entity& selectedEntity,
                       MenuBarActions& actions, bool& showDemoWindowState,
                       const InputSystem& input);

[[nodiscard]] std::string GetEditorCurrentScenePath();
void SetEditorCurrentScenePath(const std::string& path);

} // namespace Astral
