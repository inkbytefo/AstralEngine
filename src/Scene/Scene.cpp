#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Scene/Scene.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace Astral {

namespace {

void DestroyEntityCascade(Registry& registry, EntityHandle entity, std::unordered_set<EntityHandle>& visited) {
    if (!registry.IsAlive(entity) || !visited.insert(entity).second) return;

    std::vector<EntityHandle> children;
    EntityHandle parent = NullEntityHandle;
    if (registry.HasComponent<HierarchyComponent>(entity)) {
        const auto& hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        children = hierarchy.children;
        parent = hierarchy.parent;
    }

    for (EntityHandle child : children) {
        DestroyEntityCascade(registry, child, visited);
    }

    if (registry.IsAlive(parent) && registry.HasComponent<HierarchyComponent>(parent)) {
        auto& siblings = registry.GetComponent<HierarchyComponent>(parent).children;
        std::erase(siblings, entity);
    }
    registry.DestroyEntity(entity);
}

} // namespace

void Scene::DestroyEntity(EntityHandle handle) {
    std::unordered_set<EntityHandle> visited;
    DestroyEntityCascade(m_Registry, handle, visited);
}

void Scene::DestroyEntity(Entity entity) {
    assert(entity.GetScene() == this && "[Astral::Scene] Entity baska bir sahneye ait!");
    DestroyEntity(entity.GetHandle());
}

bool Scene::SetParent(EntityHandle child, EntityHandle parent) {
    if (!m_Registry.IsAlive(child) || child == parent) return false;
    if (parent != NullEntityHandle && !m_Registry.IsAlive(parent)) return false;
    if (!m_Registry.HasComponent<TransformComponent>(child) ||
        (parent != NullEntityHandle && !m_Registry.HasComponent<TransformComponent>(parent))) {
        return false;
    }

    std::unordered_set<EntityHandle> visited;
    for (EntityHandle ancestor = parent; ancestor != NullEntityHandle;) {
        if (ancestor == child || !visited.insert(ancestor).second) return false;
        if (!m_Registry.IsAlive(ancestor) || !m_Registry.HasComponent<HierarchyComponent>(ancestor)) break;
        ancestor = m_Registry.GetComponent<HierarchyComponent>(ancestor).parent;
    }

    if (!m_Registry.HasComponent<HierarchyComponent>(child)) {
        m_Registry.AddComponent<HierarchyComponent>(child, {});
    }
    if (parent != NullEntityHandle && !m_Registry.HasComponent<HierarchyComponent>(parent)) {
        m_Registry.AddComponent<HierarchyComponent>(parent, {});
    }

    auto& childHierarchy = m_Registry.GetComponent<HierarchyComponent>(child);
    const EntityHandle oldParent = childHierarchy.parent;
    if (oldParent == parent) return true;

    if (m_Registry.IsAlive(oldParent) && m_Registry.HasComponent<HierarchyComponent>(oldParent)) {
        std::erase(m_Registry.GetComponent<HierarchyComponent>(oldParent).children, child);
    }

    childHierarchy.parent = parent;
    if (parent != NullEntityHandle) {
        auto& children = m_Registry.GetComponent<HierarchyComponent>(parent).children;
        if (std::find(children.begin(), children.end(), child) == children.end()) {
            children.push_back(child);
        }
    }
    return true;
}

bool Scene::SetParent(Entity child, Entity parent) {
    if (child.GetScene() != this || parent.GetScene() != this) return false;
    return SetParent(child.GetHandle(), parent.GetHandle());
}

bool Scene::ClearParent(EntityHandle child) {
    return SetParent(child, NullEntityHandle);
}

glm::mat4 Scene::GetWorldTransform(EntityHandle entity) const {
    return GetWorldTransformMatrix(m_Registry, entity);
}

void Scene::OnRuntimeStart() {
    m_IsRunning = true;
    std::cout << "[Astral::Scene] '" << m_Name << "' Runtime baslatildi.\n";
}

void Scene::OnUpdate(float deltaTime) {
    if (!m_IsRunning) return;

    // Fizik ve Kinematik Simülasyonu (Contiguous SparseSet Traversal)
    auto& transforms = m_Registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        if (m_Registry.HasComponent<VelocityComponent>(entity)) {
            const auto& velocity = m_Registry.GetComponent<VelocityComponent>(entity);

            // Çizgisel öteleme
            transform.position += velocity.linear * deltaTime;

            // Açısal rotasyon (Euler -> Quaternion integration)
            if (glm::length(velocity.angular) > 0.0001f) {
                glm::quat deltaRot = glm::quat(velocity.angular * deltaTime);
                transform.rotation = glm::normalize(deltaRot * transform.rotation);
            }
        }
    }
}

void Scene::OnRuntimeStop() {
    m_IsRunning = false;
    std::cout << "[Astral::Scene] '" << m_Name << "' Runtime durduruldu.\n";
}

} // namespace Astral
