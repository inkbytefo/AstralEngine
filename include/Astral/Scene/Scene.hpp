#pragma once

#include "Astral/Core/Registry.hpp"
#include "Astral/Scene/Entity.hpp"
#include <string>
#include <memory>
#include <cassert>
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
    Scene() = default;
    explicit Scene(std::string name)
        : m_Name(std::move(name)) {}

    /// Derin Kopyalama (Deep-Copy Constructor): Editor'den Runtime'a gecerken sahneyi klonlar
    Scene(const Scene& other)
        : m_Registry(other.m_Registry),
          m_Name(other.m_Name),
          m_IsRunning(false) {}

    Scene& operator=(const Scene& other) {
        if (this != &other) {
            m_Registry = other.m_Registry;
            m_Name = other.m_Name;
            m_IsRunning = false;
        }
        return *this;
    }

    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;
    virtual ~Scene() = default;

    /// Sahne Klonlama Yardimcisi (Editor -> Play State)
    [[nodiscard]] static std::shared_ptr<Scene> Copy(const std::shared_ptr<Scene>& other) {
        if (!other) return nullptr;
        return std::make_shared<Scene>(*other);
    }

    // ---- Lifecycle Methods ----
    virtual void OnRuntimeStart();
    virtual void OnUpdate(float deltaTime);
    virtual void OnRuntimeStop();

    [[nodiscard]] bool IsRunning() const noexcept { return m_IsRunning; }

    // ---- Entity Factory Methods ----
    [[nodiscard]] Entity CreateEntity();
    [[nodiscard]] Entity DuplicateEntity(EntityHandle source);
    [[nodiscard]] Entity DuplicateEntity(Entity source);
    void DestroyEntity(Entity entity);
    void DestroyEntity(EntityHandle handle);
    void Clear();

    /// Yerel transformu koruyarak child'i parent altina baglar. Dongu olusacaksa false doner.
    [[nodiscard]] bool SetParent(EntityHandle child, EntityHandle parent);
    [[nodiscard]] bool SetParent(Entity child, Entity parent);
    [[nodiscard]] bool ClearParent(EntityHandle child);
    [[nodiscard]] glm::mat4 GetWorldTransform(EntityHandle entity) const;

    /// Atomic Transaction Commit: swaps internal ECS registry and state
    void Swap(Scene& other) noexcept {
        m_Registry.Swap(other.m_Registry);
        m_Name.swap(other.m_Name);
        std::swap(m_IsRunning, other.m_IsRunning);
    }

    // ---- Accessors ----
    [[nodiscard]] Registry& GetRegistry() noexcept { return m_Registry; }
    [[nodiscard]] const Registry& GetRegistry() const noexcept { return m_Registry; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_Name; }
    void SetName(std::string name) { m_Name = std::move(name); }

private:
    Registry m_Registry;
    std::string m_Name = "Untitled Scene";
    bool m_IsRunning = false;

    friend class Entity;
};

// ============================================================================
// Entity Template Forwarding Implementation (Inline Zero-Cost Abstraction)
// ============================================================================

inline bool Entity::IsValid() const noexcept {
    return m_EntityHandle != NullEntityHandle && 
           m_Scene != nullptr && 
           m_Scene->GetRegistry().IsAlive(m_EntityHandle);
}

template <typename T, typename... Args>
inline T& Entity::AddComponent(Args&&... args) {
    assert(IsValid() && "[Astral::Entity] Gecersiz Entity uzerinde AddComponent cagrildi!");
    m_Scene->GetRegistry().AddComponent<T>(m_EntityHandle, T{ std::forward<Args>(args)... });
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
}

template <typename T>
inline T& Entity::GetComponent() {
    assert(IsValid() && "[Astral::Entity] Gecersiz Entity uzerinde GetComponent cagrildi!");
    assert(HasComponent<T>() && "[Astral::Entity] Entity istenen bilesene sahip degil!");
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
}

template <typename T>
inline const T& Entity::GetComponent() const {
    assert(IsValid() && "[Astral::Entity] Gecersiz Entity uzerinde const GetComponent cagrildi!");
    assert(HasComponent<T>() && "[Astral::Entity] Entity istenen bilesene sahip degil!");
    return m_Scene->GetRegistry().GetComponent<T>(m_EntityHandle);
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
    return Entity(m_Registry.CreateEntity(), this);
}

inline void Scene::Clear() {
    m_Registry.Clear();
}

} // namespace Astral
