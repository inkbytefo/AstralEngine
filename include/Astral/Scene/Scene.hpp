#pragma once

#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Scene/Entity.hpp"
#include <string>
#include <memory>
#include <cassert>
#include <stdexcept>
#include <utility>
#include <glm/glm.hpp>

namespace Astral {

/**
 * @brief High-performance Scene container holding the ECS Registry.
 *
 * Implements strict Data-Oriented Design (DOD). Features complete deep-copy
 * mechanics via Registry::Clone() to enable isolated Editor-to-Runtime execution
 * without mutating or corrupting authoring state.
 */
class Scene {
public:
    Scene();
    explicit Scene(std::string name);

    /// Derin Kopyalama (Deep-Copy Constructor): Editor'den Runtime'a gecerken sahneyi klonlar
    Scene(const Scene& other);
    Scene& operator=(const Scene& other);

    Scene(Scene&& other) noexcept;
    Scene& operator=(Scene&& other) noexcept;
    virtual ~Scene();

    /// Sahne Klonlama Yardimcisi (Editor -> Play State)
    [[nodiscard]] static std::shared_ptr<Scene> Copy(const std::shared_ptr<Scene>& other) {
        if (!other) return nullptr;
        return std::make_shared<Scene>(*other);
    }

    [[nodiscard]] std::shared_ptr<Scene> Clone() const {
        return std::make_shared<Scene>(*this);
    }

    /// Sahne omru sorgusu (Use-After-Free korumasi)
    [[nodiscard]] static bool IsSceneAlive(const Scene* scene) noexcept;
    [[nodiscard]] static uint64_t GenerateInstanceId() noexcept;

    // ---- Lifecycle Methods ----
    virtual void OnRuntimeStart();
    virtual void OnUpdate(float deltaTime);
    virtual void OnRuntimeStop();

    [[nodiscard]] bool IsRunning() const noexcept { return m_IsRunning; }
    [[nodiscard]] uint64_t GetInstanceId() const noexcept { return m_InstanceId; }

    // ---- Entity Factory Methods ----
    [[nodiscard]] Entity CreateEntity();
    [[nodiscard]] Entity CreateEntity(std::string_view tag);
    [[nodiscard]] Entity FindEntityByTag(std::string_view tag);
    [[nodiscard]] Entity DuplicateEntity(EntityHandle source);
    [[nodiscard]] Entity DuplicateEntity(Entity source);
    void DestroyEntity(Entity entity);
    void DestroyEntity(EntityHandle handle);
    void Clear();

    /// Yerel transformu koruyarak child'i parent altina baglar. Dongu olusacaksa false doner.
    [[nodiscard]] bool SetParent(EntityHandle child, EntityHandle parent);
    [[nodiscard]] bool SetParent(Entity child, Entity parent);
    [[nodiscard]] bool ClearParent(EntityHandle child);
    [[nodiscard]] bool ClearParent(Entity child);
    [[nodiscard]] EntityHandle GetParent(EntityHandle child) const;
    [[nodiscard]] std::vector<EntityHandle> GetChildren(EntityHandle parent) const;
    [[nodiscard]] glm::mat4 GetWorldTransform(EntityHandle entity) const;

    /// Atomic Transaction Commit: swaps internal ECS registry and state
    void Swap(Scene& other) noexcept;

    // ---- Accessors ----
    [[nodiscard]] Registry& GetRegistry() noexcept { return m_Registry; }
    [[nodiscard]] const Registry& GetRegistry() const noexcept { return m_Registry; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_Name; }
    void SetName(std::string name) { m_Name = std::move(name); }

private:
    Registry m_Registry;
    std::string m_Name = "Untitled Scene";
    bool m_IsRunning = false;
    uint64_t m_InstanceId = 0;

    friend class Entity;
};

// ============================================================================
// Entity Template Forwarding Implementation (Inline Zero-Cost Abstraction)
// ============================================================================

inline Entity::Entity(EntityID handle, Scene* scene) noexcept
    : m_EntityHandle(handle),
      m_Scene(scene),
      m_SceneInstanceId(scene ? scene->GetInstanceId() : 0) {}

inline bool Entity::IsValid() const noexcept {
    return m_EntityHandle != NullEntityHandle && 
           m_Scene != nullptr && 
           m_SceneInstanceId != 0 &&
           Scene::IsSceneAlive(m_Scene) &&
           m_Scene->GetInstanceId() == m_SceneInstanceId &&
           m_Scene->GetRegistry().IsAlive(m_EntityHandle);
}

template <typename T, typename... Args>
inline T& Entity::AddComponent(Args&&... args) {
    if (!IsValid()) {
        throw std::runtime_error("[Astral::Entity] Gecersiz Entity uzerinde AddComponent cagrildi!");
    }
    m_Scene->GetRegistry().AddComponent<T>(m_EntityHandle, T{ std::forward<Args>(args)... });
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
}

template <typename T>
inline T& Entity::GetComponent() {
    if (!IsValid()) {
        throw std::runtime_error("[Astral::Entity] Gecersiz Entity uzerinde GetComponent cagrildi!");
    }
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
}

template <typename T>
inline const T& Entity::GetComponent() const {
    if (!IsValid()) {
        throw std::runtime_error("[Astral::Entity] Gecersiz Entity uzerinde const GetComponent cagrildi!");
    }
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
}

template <typename T>
inline T* Entity::TryGetComponent() noexcept {
    if (!IsValid()) return nullptr;
    return m_Scene->GetRegistry().TryGetComponent<T>(m_EntityHandle);
}

template <typename T>
inline const T* Entity::TryGetComponent() const noexcept {
    if (!IsValid()) return nullptr;
    return m_Scene->GetRegistry().TryGetComponent<T>(m_EntityHandle);
}

template <typename T>
inline bool Entity::HasComponent() const {
    if (!IsValid()) return false;
    return m_Scene->GetRegistry().HasComponent<T>(m_EntityHandle);
}

template <typename T>
inline bool Entity::RemoveComponent() {
    if (!IsValid()) return false;
    return m_Scene->GetRegistry().RemoveComponent<T>(m_EntityHandle);
}

inline Entity Scene::CreateEntity() {
    return Entity(m_Registry.CreateEntity(), this, m_InstanceId);
}

inline Entity Scene::CreateEntity(std::string_view tag) {
    Entity entity = CreateEntity();
    entity.AddComponent<TagComponent>(std::string(tag));
    return entity;
}

inline Entity Scene::FindEntityByTag(std::string_view tag) {
    auto view = m_Registry.GetView<TagComponent>();
    for (auto&& [entity, t] : view) {
        if (t.tag == tag) {
            return Entity(entity, this, m_InstanceId);
        }
    }
    return Entity();
}

inline void Scene::Clear() {
    m_InstanceId = GenerateInstanceId();
    m_Registry.Clear();
}

inline void Scene::Swap(Scene& other) noexcept {
    m_InstanceId = GenerateInstanceId();
    other.m_InstanceId = GenerateInstanceId();
    m_Registry.Swap(other.m_Registry);
    m_Name.swap(other.m_Name);
    std::swap(m_IsRunning, other.m_IsRunning);
}

} // namespace Astral
