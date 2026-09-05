#define GLM_ENABLE_EXPERIMENTAL
#include "Astral/Scene/Scene.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Core/Systems/PhysicsSubsystem.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <unordered_set>

namespace Astral {

namespace {

static std::unordered_set<const Scene*> s_AliveScenes;
static std::mutex s_AliveScenesMutex;
static std::atomic<uint64_t> s_NextInstanceId{ 1 };

void RegisterScene(const Scene* scene) {
    std::lock_guard<std::mutex> lock(s_AliveScenesMutex);
    s_AliveScenes.insert(scene);
}

void UnregisterScene(const Scene* scene) {
    std::lock_guard<std::mutex> lock(s_AliveScenesMutex);
    s_AliveScenes.erase(scene);
}

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

bool Scene::IsSceneAlive(const Scene* scene) noexcept {
    if (!scene) return false;
    std::lock_guard<std::mutex> lock(s_AliveScenesMutex);
    return s_AliveScenes.contains(scene);
}

uint64_t Scene::GenerateInstanceId() noexcept {
    return s_NextInstanceId.fetch_add(1, std::memory_order_relaxed);
}

Scene::Scene()
    : m_InstanceId(GenerateInstanceId()) {
    RegisterScene(this);
}

Scene::Scene(std::string name)
    : m_Name(std::move(name)),
      m_InstanceId(GenerateInstanceId()) {
    RegisterScene(this);
}

Scene::Scene(const Scene& other)
    : m_Registry(other.m_Registry),
      m_Name(other.m_Name),
      m_IsRunning(false),
      m_InstanceId(GenerateInstanceId()) {
    RegisterScene(this);
}

Scene& Scene::operator=(const Scene& other) {
    if (this != &other) {
        m_Registry = other.m_Registry;
        m_Name = other.m_Name;
        m_IsRunning = false;
        m_InstanceId = GenerateInstanceId();
    }
    return *this;
}

Scene::Scene(Scene&& other) noexcept
    : m_Registry(std::move(other.m_Registry)),
      m_Name(std::move(other.m_Name)),
      m_IsRunning(other.m_IsRunning),
      m_InstanceId(other.m_InstanceId) {
    other.m_InstanceId = 0;
    RegisterScene(this);
}

Scene& Scene::operator=(Scene&& other) noexcept {
    if (this != &other) {
        m_Registry = std::move(other.m_Registry);
        m_Name = std::move(other.m_Name);
        m_IsRunning = other.m_IsRunning;
        m_InstanceId = other.m_InstanceId;
        other.m_InstanceId = 0;
    }
    return *this;
}

Scene::~Scene() {
    UnregisterScene(this);
}

Entity Scene::DuplicateEntity(EntityHandle source) {
    if (!m_Registry.IsAlive(source)) return Entity();

    Entity newEntity = CreateEntity();

    // TagComponent
    if (m_Registry.HasComponent<TagComponent>(source)) {
        const auto& tag = m_Registry.GetComponent<TagComponent>(source);
        newEntity.AddComponent<TagComponent>(tag.tag + " (Copy)");
    }

    // TransformComponent (Hafifçe ötelenmiş: +0.5m x ve z ekseninde)
    if (m_Registry.HasComponent<TransformComponent>(source)) {
        auto transform = m_Registry.GetComponent<TransformComponent>(source);
        transform.position += glm::vec3(0.5f, 0.0f, 0.5f);
        newEntity.AddComponent<TransformComponent>(transform);
    }

    // SDFComponent
    if (m_Registry.HasComponent<SDFComponent>(source)) {
        newEntity.AddComponent<SDFComponent>(m_Registry.GetComponent<SDFComponent>(source));
    }

    if (m_Registry.HasComponent<CameraComponent>(source)) {
        auto camera = m_Registry.GetComponent<CameraComponent>(source);
        camera.primary = 0; // Duplicating a camera must not steal the active view.
        newEntity.AddComponent<CameraComponent>(camera);
    }

    // VelocityComponent
    if (m_Registry.HasComponent<VelocityComponent>(source)) {
        newEntity.AddComponent<VelocityComponent>(m_Registry.GetComponent<VelocityComponent>(source));
    }

    // HealthComponent
    if (m_Registry.HasComponent<HealthComponent>(source)) {
        newEntity.AddComponent<HealthComponent>(m_Registry.GetComponent<HealthComponent>(source));
    }

    // VisibilityComponent
    if (m_Registry.HasComponent<VisibilityComponent>(source)) {
        newEntity.AddComponent<VisibilityComponent>(m_Registry.GetComponent<VisibilityComponent>(source));
    }

    // Eger kaynagin bir parent'i varsa kopyayi da ayni parent altina al
    if (m_Registry.HasComponent<HierarchyComponent>(source)) {
        EntityHandle parent = m_Registry.GetComponent<HierarchyComponent>(source).parent;
        if (parent != NullEntityHandle && m_Registry.IsAlive(parent)) {
            (void)SetParent(newEntity.GetHandle(), parent);
        }
    }

    return newEntity;
}

Entity Scene::DuplicateEntity(Entity source) {
    if (!source.IsValid() || source.GetScene() != this) return Entity();
    return DuplicateEntity(source.GetHandle());
}

void Scene::DestroyEntity(EntityHandle handle) {
    std::unordered_set<EntityHandle> visited;
    DestroyEntityCascade(m_Registry, handle, visited);
}

void Scene::DestroyEntity(Entity entity) {
    if (!entity.IsValid() || entity.GetScene() != this) return;
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
    if (!child.IsValid() || child.GetScene() != this) return false;
    if (parent.GetHandle() != NullEntityHandle) {
        if (!parent.IsValid() || parent.GetScene() != this) return false;
    }
    return SetParent(child.GetHandle(), parent.GetHandle());
}

bool Scene::ClearParent(EntityHandle child) {
    return SetParent(child, NullEntityHandle);
}

bool Scene::ClearParent(Entity child) {
    if (!child.IsValid() || child.GetScene() != this) return false;
    return ClearParent(child.GetHandle());
}

EntityHandle Scene::GetParent(EntityHandle child) const {
    if (!m_Registry.HasComponent<HierarchyComponent>(child)) return NullEntityHandle;
    return m_Registry.GetComponent<HierarchyComponent>(child).parent;
}

std::vector<EntityHandle> Scene::GetChildren(EntityHandle parent) const {
    if (!m_Registry.HasComponent<HierarchyComponent>(parent)) return {};
    return m_Registry.GetComponent<HierarchyComponent>(parent).children;
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
    PhysicsSubsystem::Integrate(m_Registry, deltaTime);
}

void Scene::OnRuntimeStop() {
    m_IsRunning = false;
    std::cout << "[Astral::Scene] '" << m_Name << "' Runtime durduruldu.\n";
}

} // namespace Astral
