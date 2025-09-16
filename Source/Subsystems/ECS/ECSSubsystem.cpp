#include "ECSSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"

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
    
    // Create test entity with Transform and Render components
    uint32_t testEntity = CreateEntity();
    
    // Add Transform component
    auto& transform = AddComponent<TransformComponent>(testEntity);
    transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // Add Render component
    auto& render = AddComponent<RenderComponent>(testEntity);
    render.modelPath = "default_triangle";
    render.materialPath = "default";
    render.visible = true;
    render.renderLayer = 0;
    
    // Add Name component for debugging
    auto& name = AddComponent<NameComponent>(testEntity);
    name.name = "TestTriangle";
    
    Logger::Info("ECSSubsystem", "Created test entity {} with Transform and Render components", testEntity);
    Logger::Info("ECSSubsystem", "ECS subsystem initialized successfully");
}

void ECSSubsystem::OnUpdate(float deltaTime) {
    // Update all entities with Transform components
    auto entitiesWithTransform = QueryEntities<TransformComponent>();
    
    for (uint32_t entity : entitiesWithTransform) {
        if (HasComponent<TransformComponent>(entity)) {
            auto& transform = GetComponent<TransformComponent>(entity);
            
            // Simple rotation animation for testing
            transform.rotation.y += deltaTime * 0.5f; // Rotate 0.5 radians per second
            
            // Keep rotation in reasonable range
            if (transform.rotation.y > glm::two_pi<float>()) {
                transform.rotation.y -= glm::two_pi<float>();
            }
        }
    }
    
    Logger::Debug("ECSSubsystem", "Updated {} entities with deltaTime: {}", entitiesWithTransform.size(), deltaTime);
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
    auto& indices = m_entityComponentIndices[type];
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        return nullptr;
    }
    
    auto poolIt = m_componentPools.find(type);
    if (poolIt == m_componentPools.end()) {
        return nullptr;
    }
    
    auto& pool = poolIt->second;
    return &pool.data[it->second * pool.componentSize];
}

const void* ECSSubsystem::GetComponentPointer(uint32_t entity, std::type_index type) const {
    auto indicesIt = m_entityComponentIndices.find(type);
    if (indicesIt == m_entityComponentIndices.end()) {
        return nullptr;
    }
    
    auto& indices = indicesIt->second;
    auto it = indices.find(entity);
    
    if (it == indices.end()) {
        return nullptr;
    }
    
    auto poolIt = m_componentPools.find(type);
    if (poolIt == m_componentPools.end()) {
        return nullptr;
    }
    
    auto& pool = poolIt->second;
    return &pool.data[it->second * pool.componentSize];
}

ECSSubsystem::RenderPacket ECSSubsystem::GetRenderData() {
    RenderPacket packet;
    
    // Query entities with both Transform and Render components
    auto renderableEntities = QueryEntities<TransformComponent, RenderComponent>();
    
    for (uint32_t entity : renderableEntities) {
        auto& transform = GetComponent<TransformComponent>(entity);
        auto& render = GetComponent<RenderComponent>(entity);
        
        if (render.visible) {
            RenderPacket::RenderItem item;
            item.transform = transform.GetWorldMatrix();
            item.modelPath = render.modelPath;
            item.materialPath = render.materialPath;
            item.visible = render.visible;
            item.renderLayer = render.renderLayer;
            
            packet.renderItems.push_back(item);
        }
    }
    
    // Sort by render layer
    std::sort(packet.renderItems.begin(), packet.renderItems.end(), 
        [](const RenderPacket::RenderItem& a, const RenderPacket::RenderItem& b) {
            return a.renderLayer < b.renderLayer;
        });
    
    Logger::Debug("ECSSubsystem", "Collected render data for {} items", packet.renderItems.size());
    
    return packet;
}

} // namespace AstralEngine
