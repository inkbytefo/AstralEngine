#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include <unordered_set>
#include <vector>

namespace Astral {

/// Scene Hierarchy (Outliner) panel.
/// Displays all entities in a tree/list with selection, add/delete, and context menu.
class SceneHierarchy {
public:
    void Draw(Scene& scene, Entity& selectedEntity);

private:
    void DrawEntityNode(Scene& scene, EntityHandle entity, Entity& selectedEntity);

    std::vector<EntityHandle> m_Roots;
    std::unordered_set<EntityHandle> m_Visited;
    EntityHandle m_PendingDelete = NullEntityHandle;
    EntityHandle m_PendingChild = NullEntityHandle;
    EntityHandle m_PendingParent = NullEntityHandle;
    bool m_HasPendingReparent = false;
    bool m_ReparentRejected = false;
};

} // namespace Astral
