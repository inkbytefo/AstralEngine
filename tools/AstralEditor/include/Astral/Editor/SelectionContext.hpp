#pragma once
#include "Astral/Scene/Scene.hpp"
#include <algorithm>
#include <vector>

namespace Astral {
// Ordered selection: the last selected live entity is the primary focus.
class SelectionContext {
public:
    void SelectEntity(Entity entity, bool additive = false) {
        if (!entity.IsValid()) { if (!additive) ClearSelection(); return; }
        if (!additive || (m_Primary.IsValid() && m_Primary.GetScene() != entity.GetScene())) ClearSelection();
        if (!IsEntitySelected(entity)) m_Entities.push_back(entity);
        m_Primary = entity;
    }
    void DeselectEntity(Entity entity) {
        std::erase(m_Entities, entity);
        if (m_Primary == entity) m_Primary = m_Entities.empty() ? Entity() : m_Entities.back();
    }
    void Toggle(Entity entity) { if (IsEntitySelected(entity)) DeselectEntity(entity); else SelectEntity(entity, true); }
    void ClearSelection() { m_Entities.clear(); m_Primary = Entity(); }
    bool IsEntitySelected(Entity entity) const { return std::find(m_Entities.begin(), m_Entities.end(), entity) != m_Entities.end(); }
    const std::vector<Entity>& Entities() const { return m_Entities; }
    Entity Primary() const { return m_Primary; }
    // Stable storage for existing create/rename commands; reconcile after legacy UI calls.
    Entity& PrimaryStorage() { return m_Primary; }
    void Reconcile() {
        if (m_Primary.GetHandle() == NullEntityHandle) { ClearSelection(); return; }
        std::erase_if(m_Entities, [](Entity e) { return !e.IsValid(); });
        if (!m_Primary.IsValid()) m_Primary = m_Entities.empty() ? Entity() : m_Entities.back();
        else if (!IsEntitySelected(m_Primary)) SelectEntity(m_Primary);
    }
private:
    std::vector<Entity> m_Entities;
    Entity m_Primary;
};
}
