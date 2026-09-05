#pragma once

#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/Entity.hpp"
#include <unordered_set>
#include <vector>
#include <string>

namespace Astral {

/**
 * @brief AAA standardinda gelismis Sahne Agaci (Scene Hierarchy / World Outliner) paneli.
 *
 * Unity ve Unreal Engine pratiklerini uygular:
 * - Canli arama ve filtreleme
 * - Hizli gorunurluk (Eye Toggle)
 * - Nesne klonlama (Duplicate - Ctrl+D)
 * - Gelismis context menu (Yeni alt nesne, yeniden adlandirma, silme)
 * - Surukle-birak ile ebeveyn atama ve bosa birakarak koke donme (Unparent)
 * - O(N) tek gecisli yuksek performansli agac render'i
 */
class CommandStack;

class SceneHierarchy {
public:
    SceneHierarchy() = default;
    ~SceneHierarchy() = default;

    void Draw(Scene& scene, Entity& selectedEntity, CommandStack* commandStack = nullptr);

private:
    void DrawEntityNode(Scene& scene, EntityHandle entity, Entity& selectedEntity, bool isFiltered);
    bool EntityMatchesFilter(Scene& scene, EntityHandle entity) const;
    bool NodeOrDescendantMatchesFilter(Scene& scene, EntityHandle entity) const;

    std::vector<EntityHandle> m_Roots;
    std::unordered_set<EntityHandle> m_Visited;

    // Bekleyen eylemler (Deferred actions to avoid iterator invalidation)
    EntityHandle m_PendingDelete = NullEntityHandle;
    EntityHandle m_PendingDuplicate = NullEntityHandle;
    EntityHandle m_PendingChild = NullEntityHandle;
    EntityHandle m_PendingParent = NullEntityHandle;
    bool m_HasPendingReparent = false;
    bool m_ReparentRejected = false;

    // Primitif ekleme eylemi
    int m_PendingAddPrimitive = -1; // -1: Yok, 0: Sphere, 1: Box, 2: Torus, 3: Plane, 4: Capsule, 5: Cylinder
    EntityHandle m_PendingAddToParent = NullEntityHandle;

    // Arama ve filtreleme
    char m_SearchFilter[128] = "";

    // Satir ici yeniden adlandirma (Rename)
    EntityHandle m_RenamingEntity = NullEntityHandle;
    char m_RenameBuffer[128] = "";

    CommandStack* m_CommandStack = nullptr;
};

} // namespace Astral
