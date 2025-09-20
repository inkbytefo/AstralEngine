#include "ECSSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include <glm/glm.hpp>
#include <algorithm> // For std::sort

namespace AstralEngine {

ECSSubsystem::ECSSubsystem() 
    : m_nextEntityId(1), m_owner(nullptr) {
    Logger::Debug("ECSSubsystem", "ECSSubsystem created");
}

ECSSubsystem::~ECSSubsystem() {
    Logger::Debug("ECSSubsystem", "ECSSubsystem destroyed");
}

void ECSSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("ECSSubsystem", "Initializing ECS subsystem...");
    // Test entity creation removed. This should be handled by a sandbox application.
    Logger::Info("ECSSubsystem", "ECS subsystem initialized successfully");
}

void ECSSubsystem::OnUpdate(float deltaTime) {
    // Test animation logic removed. Entity transformations should be handled by dedicated systems.
    Logger::Debug("ECSSubsystem", "ECSSubsystem OnUpdate called with deltaTime: {}", deltaTime);
}

void ECSSubsystem::OnShutdown() {
    Logger::Info("ECSSubsystem", "Shutting down ECS subsystem...");
    
    // Clear all entities and components
    m_entities.clear();
    m_freeEntities.clear();
    m_componentPools.clear();
    m_entityComponentIndices.clear();
    m_nextEntityId = 1;
    
    Logger::Info("ECSSubsystem", "ECS subsystem shutdown complete");
}

uint32_t ECSSubsystem::CreateEntity() {
    uint32_t entity;
    
    // Reuse free entity IDs if available
    if (!m_freeEntities.empty()) {
        entity = m_freeEntities.back();
        m_freeEntities.pop_back();
    } else {
        entity = m_nextEntityId++;
    }
    
    m_entities.push_back(entity);
    Logger::Debug("ECSSubsystem", "Created entity: {}", entity);
    
    return entity;
}

void ECSSubsystem::DestroyEntity(uint32_t entity) {
    auto it = std::find(m_entities.begin(), m_entities.end(), entity);
    if (it == m_entities.end()) {
        Logger::Warning("ECSSubsystem", "Attempted to destroy invalid entity: {}", entity);
        return;
    }
    
    // Remove all components for this entity
    for (auto& pair : m_entityComponentIndices) {
        auto& indices = pair.second;
        indices.erase(entity);
    }
    
    // Remove from entities list and add to free list
    m_entities.erase(it);
    m_freeEntities.push_back(entity);
    
    Logger::Debug("ECSSubsystem", "Destroyed entity: {}", entity);
}

bool ECSSubsystem::IsEntityValid(uint32_t entity) const {
    return std::find(m_entities.begin(), m_entities.end(), entity) != m_entities.end();
}

void* ECSSubsystem::GetComponentPointer(uint32_t entity, std::type_index type) {
    // Check if entity is valid
    if (!IsEntityValid(entity)) {
        Logger::Warning("ECSSubsystem", "Attempted to get component from invalid entity: {}", entity);
        return nullptr;
    }
    
    // Check if component type exists in indices
    auto indicesIt = m_entityComponentIndices.find(type);
    if (indicesIt == m_entityComponentIndices.end()) {
        return nullptr;
    }
    
    auto& indices = indicesIt->second;
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        return nullptr;
    }
    
    // Check if component pool exists
    auto poolIt = m_componentPools.find(type);
    if (poolIt == m_componentPools.end()) {
        Logger::Error("ECSSubsystem", "Component pool not found for type: {}", type.name());
        return nullptr;
    }
    
    auto& pool = poolIt->second;
    
    // Validate index bounds
    if (it->second >= pool.count) {
        Logger::Error("ECSSubsystem", "Component index out of bounds for entity: {}", entity);
        return nullptr;
    }
    
    return &pool.data[it->second * pool.componentSize];
}

const void* ECSSubsystem::GetComponentPointer(uint32_t entity, std::type_index type) const {
    // Check if entity is valid
    if (!IsEntityValid(entity)) {
        Logger::Warning("ECSSubsystem", "Attempted to get component from invalid entity: {}", entity);
        return nullptr;
    }
    
    // Check if component type exists in indices
    auto indicesIt = m_entityComponentIndices.find(type);
    if (indicesIt == m_entityComponentIndices.end()) {
        return nullptr;
    }
    
    auto& indices = indicesIt->second;
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        return nullptr;
    }
    
    // Check if component pool exists
    auto poolIt = m_componentPools.find(type);
    if (poolIt == m_componentPools.end()) {
        Logger::Error("ECSSubsystem", "Component pool not found for type: {}", type.name());
        return nullptr;
    }
    
    auto& pool = poolIt->second;
    
    // Validate index bounds
    if (it->second >= pool.count) {
        Logger::Error("ECSSubsystem", "Component index out of bounds for entity: {}", entity);
        return nullptr;
    }
    
    return &pool.data[it->second * pool.componentSize];
}

ECSSubsystem::RenderPacket ECSSubsystem::GetRenderData() {
    RenderPacket packet;
    
    // Query entities with both Transform and Render components
    auto renderableEntities = QueryEntities<TransformComponent, RenderComponent>();
    
    for (uint32_t entity : renderableEntities) {
        auto* transform = GetComponent<TransformComponent>(entity);
        auto* render = GetComponent<RenderComponent>(entity);
        
        if (!transform || !render) {
            Logger::Warning("ECSSubsystem", "Entity {} missing required components for rendering", entity);
            continue;
        }
        
        // Sadece visible olan entity'leri topla
        if (render->visible) {
            // Material-driven rendering: RenderItem sadece gerekli bilgileri içerir
            RenderPacket::RenderItem item(
                transform->GetWorldMatrix(),
                render->GetModelHandle(),
                render->GetMaterialHandle(),
                render->visible,
                render->renderLayer
            );
            
            packet.renderItems.push_back(item);
            
            Logger::Debug("ECSSubsystem", "Added entity {} to render packet (model: {}, material: {})",
                         entity,
                         item.modelHandle.IsValid() ? std::to_string(item.modelHandle.GetID()).c_str() : "invalid",
                         item.materialHandle.IsValid() ? std::to_string(item.materialHandle.GetID()).c_str() : "invalid");
        }
    }
    
    // Sort by render layer
    std::sort(packet.renderItems.begin(), packet.renderItems.end(), 
        [](const RenderPacket::RenderItem& a, const RenderPacket::RenderItem& b) {
            return a.renderLayer < b.renderLayer;
        });
    
    Logger::Debug("ECSSubsystem", "Collected render data for {} items (total entities: {})", 
                 packet.renderItems.size(), renderableEntities.size());
    
    return packet;
}

} // namespace AstralEngine
