#pragma once

#include "Astral/Core/Registry.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace Astral {

class Scene;

/**
 * @brief Zero-overhead lightweight entity handle.
 * 
 * Complies with strict Data-Oriented Design (DOD). It is NOT an OOP base class
 * and contains no virtual dispatch. Acts strictly as an index handle into the
 * owning Scene's contiguous SparseSet Registry.
 */
class Entity final {
public:
    static constexpr EntityID NullEntity = static_cast<EntityID>(-1);

    constexpr Entity() noexcept = default;
    Entity(EntityID handle, Scene* scene) noexcept;
    constexpr Entity(EntityID handle, Scene* scene, uint64_t sceneInstanceId) noexcept
        : m_EntityHandle(handle), m_Scene(scene), m_SceneInstanceId(sceneInstanceId) {}

    Entity(const Entity&) noexcept = default;
    Entity& operator=(const Entity&) noexcept = default;
    Entity(Entity&&) noexcept = default;
    Entity& operator=(Entity&&) noexcept = default;
    ~Entity() = default;

    // ---- Lifecycle & Validity ----
    [[nodiscard]] bool IsValid() const noexcept;

    [[nodiscard]] explicit operator bool() const noexcept {
        return IsValid();
    }

    [[nodiscard]] constexpr EntityHandle GetHandle() const noexcept {
        return m_EntityHandle;
    }

    [[nodiscard]] constexpr operator EntityHandle() const noexcept {
        return m_EntityHandle;
    }

    [[nodiscard]] constexpr EntityIndex GetIndex() const noexcept {
        return GetEntityIndex(m_EntityHandle);
    }

    [[nodiscard]] constexpr EntityGeneration GetGeneration() const noexcept {
        return GetEntityGeneration(m_EntityHandle);
    }

    [[nodiscard]] constexpr uint32_t GetID() const noexcept {
        return GetIndex();
    }

    [[nodiscard]] constexpr uint64_t GetSceneInstanceId() const noexcept {
        return m_SceneInstanceId;
    }

    [[nodiscard]] std::string ToDisplayString() const {
        return "Entity #" + std::to_string(GetIndex()) + " (gen " + std::to_string(GetGeneration()) + ")";
    }

    [[nodiscard]] constexpr Scene* GetScene() const noexcept {
        return m_Scene;
    }

    [[nodiscard]] friend constexpr bool operator==(const Entity& lhs, const Entity& rhs) noexcept {
        return lhs.m_EntityHandle == rhs.m_EntityHandle && 
               lhs.m_Scene == rhs.m_Scene && 
               lhs.m_SceneInstanceId == rhs.m_SceneInstanceId;
    }

    [[nodiscard]] friend constexpr bool operator!=(const Entity& lhs, const Entity& rhs) noexcept {
        return !(lhs == rhs);
    }

    // ---- Component Operations (Inline Template Forwarding) ----

    template <typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template <typename T>
    [[nodiscard]] T& GetComponent();

    template <typename T>
    [[nodiscard]] const T& GetComponent() const;

    template <typename T>
    [[nodiscard]] T* TryGetComponent() noexcept;

    template <typename T>
    [[nodiscard]] const T* TryGetComponent() const noexcept;

    template <typename T>
    [[nodiscard]] bool HasComponent() const;

    template <typename T>
    bool RemoveComponent();

private:
    EntityID m_EntityHandle{ NullEntity };
    Scene* m_Scene{ nullptr };
    uint64_t m_SceneInstanceId{ 0 };
};

} // namespace Astral
