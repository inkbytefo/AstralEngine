#pragma once

#include "Scene.h"
#include <entt/entt.hpp>
#include "../../Core/Logger.h"

namespace AstralEngine {

    class Entity {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene)
            : m_EntityHandle(handle), m_Scene(scene) {}
        Entity(const Entity& other) = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) {
            if (HasComponent<T>()) {
                Logger::Warning("Entity", "Entity already has component!");
                return GetComponent<T>();
            }
            return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent() {
            if (!HasComponent<T>()) {
                Logger::Error("Entity", "Entity does not have component!");
                // This will crash if we return garbage, but strict checking requires valid logic
                // For now, assume it exists or check HasComponent before calling
            }
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template<typename T>
        bool HasComponent() {
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template<typename T>
        T* TryGetComponent() {
            return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
        }

        template<typename T>
        void RemoveComponent() {
            if (!HasComponent<T>()) {
                 Logger::Warning("Entity", "Entity does not have component to remove!");
                 return;
            }
            m_Scene->m_Registry.remove<T>(m_EntityHandle);
        }

        operator bool() const { return m_EntityHandle != entt::null; }
        operator entt::entity() const { return m_EntityHandle; }
        operator uint32_t() const { return (uint32_t)m_EntityHandle; }

        UUID GetUUID() { return GetComponent<IDComponent>().ID; }
        const std::string& GetName() { return GetComponent<NameComponent>().name; }

        bool operator==(const Entity& other) const {
            return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const {
            return !(*this == other);
        }

    private:
        entt::entity m_EntityHandle{ entt::null };
        Scene* m_Scene = nullptr;
    };

}
