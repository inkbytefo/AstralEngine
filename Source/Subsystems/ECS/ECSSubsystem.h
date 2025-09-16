#pragma once

#include "../../Core/ISubsystem.h"
#include "../../ECS/Components.h"
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <cstdint>

namespace AstralEngine {

class Engine;

/**
 * @brief Entity Component System (ECS) Subsystem
 * 
 * Oyun dünyasının durumunu yönetir. Entity, Component ve System'lerin
 * yaşam döngüsünden sorumludur. Veri odaklı tasarım ilkesine göre
 * çalışır ve render verilerini RenderSubsystem'e gönderir.
 */
class ECSSubsystem : public ISubsystem {
public:
    ECSSubsystem();
    ~ECSSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "ECSSubsystem"; }

    // Entity management
    uint32_t CreateEntity();
    void DestroyEntity(uint32_t entity);
    bool IsEntityValid(uint32_t entity) const;

    // Component management
    template<typename T>
    T& AddComponent(uint32_t entity);
    
    template<typename T>
    T& GetComponent(uint32_t entity);
    
    template<typename T>
    bool HasComponent(uint32_t entity) const;
    
    template<typename T>
    void RemoveComponent(uint32_t entity);

    // Entity queries
    template<typename... Components>
    std::vector<uint32_t> QueryEntities();

    // Render data collection
    struct RenderPacket {
        struct RenderItem {
            glm::mat4 transform;
            std::string modelPath;
            std::string materialPath;
            bool visible;
            int renderLayer;
        };
        std::vector<RenderItem> renderItems;
    };

    RenderPacket GetRenderData();

private:
    // Component storage
    struct ComponentPool {
        std::vector<std::byte> data;
        size_t componentSize;
        size_t count;
        
        ComponentPool() : componentSize(0), count(0) {}
        ComponentPool(size_t size) : componentSize(size), count(0) {}
    };

    // Entity management
    std::vector<uint32_t> m_entities;
    std::vector<uint32_t> m_freeEntities;
    uint32_t m_nextEntityId;

    // Component storage
    std::unordered_map<std::type_index, ComponentPool> m_componentPools;
    std::unordered_map<std::type_index, std::unordered_map<uint32_t, size_t>> m_entityComponentIndices;

    // Engine reference
    Engine* m_owner;

    // Helper methods
    template<typename T>
    ComponentPool& GetComponentPool();
    
    template<typename T>
    std::unordered_map<uint32_t, size_t>& GetComponentIndices();
    
    template<typename T>
    const std::unordered_map<uint32_t, size_t>& GetComponentIndices() const;
    
    void* GetComponentPointer(uint32_t entity, std::type_index type);
    const void* GetComponentPointer(uint32_t entity, std::type_index type) const;
};

// Template implementations
template<typename T>
T& ECSSubsystem::AddComponent(uint32_t entity) {
    if (!IsEntityValid(entity)) {
        // TODO: Proper error handling
        static T dummy;
        return dummy;
    }

    auto& pool = GetComponentPool<T>();
    auto& indices = GetComponentIndices<T>();

    // Check if component already exists
    if (indices.find(entity) != indices.end()) {
        return GetComponent<T>(entity);
    }

    // Add new component
    size_t index = pool.count;
    pool.data.resize(pool.data.size() + sizeof(T));
    T* component = new (&pool.data[index * sizeof(T)]) T();
    
    indices[entity] = index;
    pool.count++;

    return *component;
}

template<typename T>
T& ECSSubsystem::GetComponent(uint32_t entity) {
    auto& indices = GetComponentIndices<T>();
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        // TODO: Proper error handling
        static T dummy;
        return dummy;
    }

    auto& pool = GetComponentPool<T>();
    return *reinterpret_cast<T*>(&pool.data[it->second * sizeof(T)]);
}

template<typename T>
bool ECSSubsystem::HasComponent(uint32_t entity) const {
    auto& indices = GetComponentIndices<T>();
    return indices.find(entity) != indices.end();
}

template<typename T>
void ECSSubsystem::RemoveComponent(uint32_t entity) {
    auto& indices = GetComponentIndices<T>();
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        return;
    }

    auto& pool = GetComponentPool<T>();
    size_t index = it->second;
    
    // Move last component to this position (if not last)
    if (index != pool.count - 1) {
        T* lastComponent = reinterpret_cast<T*>(&pool.data[(pool.count - 1) * sizeof(T)]);
        new (&pool.data[index * sizeof(T)]) T(std::move(*lastComponent));
        lastComponent->~T();
        
        // Update index for moved component
        for (auto& pair : indices) {
            if (pair.second == pool.count - 1) {
                pair.second = index;
                break;
            }
        }
    } else {
        // Just destroy the component
        T* component = reinterpret_cast<T*>(&pool.data[index * sizeof(T)]);
        component->~T();
    }
    
    indices.erase(it);
    pool.count--;
    pool.data.resize(pool.count * sizeof(T));
}

template<typename... Components>
std::vector<uint32_t> ECSSubsystem::QueryEntities() {
    std::vector<uint32_t> result;
    
    // Start with all entities
    if (m_entities.empty()) {
        return result;
    }
    
    result = m_entities;
    
    // Filter by each component type
    ([&result, this]() {
        std::vector<uint32_t> filtered;
        auto& indices = GetComponentIndices<Components>();
        
        for (uint32_t entity : result) {
            if (indices.find(entity) != indices.end()) {
                filtered.push_back(entity);
            }
        }
        
        result = filtered;
    }(), ...);
    
    return result;
}

template<typename T>
ECSSubsystem::ComponentPool& ECSSubsystem::GetComponentPool() {
    std::type_index type = typeid(T);
    auto it = m_componentPools.find(type);
    
    if (it == m_componentPools.end()) {
        m_componentPools.emplace(type, ComponentPool(sizeof(T)));
    }
    
    return m_componentPools[type];
}

template<typename T>
std::unordered_map<uint32_t, size_t>& ECSSubsystem::GetComponentIndices() {
    std::type_index type = typeid(T);
    return m_entityComponentIndices[type];
}

template<typename T>
const std::unordered_map<uint32_t, size_t>& ECSSubsystem::GetComponentIndices() const {
    std::type_index type = typeid(T);
    return m_entityComponentIndices.at(type);
}

} // namespace AstralEngine
