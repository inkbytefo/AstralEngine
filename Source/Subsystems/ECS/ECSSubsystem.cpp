#include "ECSSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "../Renderer/Buffers/VulkanMesh.h"
#include "../Renderer/RendererTypes.h"
#include "../Renderer/Core/VulkanDevice.h"
#include "../Renderer/RenderSubsystem.h"
#include <glm/glm.hpp>

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
    
    // Create default triangle mesh
    Logger::Info("ECSSubsystem", "Creating default triangle mesh...");
    
    // Get Vulkan device from RenderSubsystem
    auto renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
    if (!renderSubsystem) {
        Logger::Error("ECSSubsystem", "RenderSubsystem not found - cannot create mesh");
        return;
    }
    
    auto graphicsDevice = renderSubsystem->GetGraphicsDevice();
    if (!graphicsDevice) {
        Logger::Error("ECSSubsystem", "GraphicsDevice not found - cannot create mesh");
        return;
    }
    
    auto vulkanDevice = graphicsDevice->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("ECSSubsystem", "VulkanDevice not found - cannot create mesh");
        return;
    }
    
    // Create triangle vertices and indices
    std::vector<Vertex> vertices = {
        {{0.0f, -0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},  // Bottom - white
        {{0.8f, 0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},   // Right top - white  
        {{-0.8f, 0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}   // Left top - white
    };
    
    std::vector<uint32_t> indices = {0, 1, 2}; // Triangle indices
    
    // Create VulkanMesh
    auto triangleMesh = std::make_shared<VulkanMesh>();
    if (!triangleMesh->Initialize(vulkanDevice, vertices, indices)) {
        Logger::Error("ECSSubsystem", "Failed to initialize triangle mesh: {}", triangleMesh->GetLastError());
        return;
    }
    
    Logger::Info("ECSSubsystem", "Triangle mesh created successfully with {} vertices and {} indices", 
                 vertices.size(), indices.size());
    
    // Store the mesh in the render component for future reference
    // Note: We'll pass it through the render packet in GetRenderData
    
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
    
    // Create default triangle mesh for the default_triangle model
    // This is a temporary solution - in a real implementation, we would load meshes from AssetManager
    std::shared_ptr<VulkanMesh> defaultTriangleMesh = nullptr;
    
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
            
            // Create mesh for default_triangle model
            if (render.modelPath == "default_triangle" && !defaultTriangleMesh) {
                Logger::Debug("ECSSubsystem", "Creating default triangle mesh for entity {}", entity);
                
                // Get Vulkan device from RenderSubsystem
                auto renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
                if (renderSubsystem && renderSubsystem->GetGraphicsDevice()) {
                    auto vulkanDevice = renderSubsystem->GetGraphicsDevice()->GetVulkanDevice();
                    if (vulkanDevice) {
                        // Create triangle vertices and indices
                        std::vector<Vertex> vertices = {
                            {{0.0f, -0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.0f}},  // Bottom - white
                            {{0.8f, 0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},   // Right top - white  
                            {{-0.8f, 0.8f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}   // Left top - white
                        };
                        
                        std::vector<uint32_t> indices = {0, 1, 2}; // Triangle indices
                        
                        // Create VulkanMesh
                        defaultTriangleMesh = std::make_shared<VulkanMesh>();
                        if (!defaultTriangleMesh->Initialize(vulkanDevice, vertices, indices)) {
                            Logger::Error("ECSSubsystem", "Failed to initialize triangle mesh in GetRenderData: {}", 
                                         defaultTriangleMesh->GetLastError());
                            defaultTriangleMesh = nullptr;
                        } else {
                            Logger::Debug("ECSSubsystem", "Default triangle mesh created successfully");
                        }
                    }
                }
            }
            
            // Assign the mesh to the render item
            item.mesh = defaultTriangleMesh;
            
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
