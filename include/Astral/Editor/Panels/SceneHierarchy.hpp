#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"

namespace Astral {

/// Scene Hierarchy (Outliner) panel.
/// Displays all entities in a tree/list with selection, add/delete, and context menu.
class SceneHierarchy {
public:
    void Draw(Scene& scene, Entity& selectedEntity);
};

} // namespace Astral
