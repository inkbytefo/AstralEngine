#pragma once

#include "IRenderer.h"
#include "Core/Engine.h"
#include "Camera.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declarations
namespace AstralEngine {
class GraphicsDevice;
class VulkanShader;
class VulkanPipeline;
class VulkanBuffer;
class VulkanMesh;
class VulkanTexture;
class AssetManager;
class Model;
}

namespace AstralEngine {

/**
 * @class VulkanRenderer
 * @brief Basitleştirilmiş Vulkan Renderer implementasyonu
 * 
 * Yeni mimariye göre basitleştirilmiş renderer. GraphicsDevice ve RenderSubsystem
 * ile entegre çalışır. Sadece rendering-specific işlemleri yönetir.
 */
class VulkanRenderer : public IRenderer {
public:
    struct Config {
        bool enableValidationLayers = true;
    };

    // Vertex structure is now defined in RendererTypes.h
    // Using the unified Vertex structure with 3D position and texture coordinates

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    VulkanRenderer();
    virtual ~VulkanRenderer();


    // IRenderer interface implementation
    bool Initialize(GraphicsDevice* device, void* owner = nullptr) override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Submit(const RenderCommand& command) override;
    void SubmitCommands(const std::vector<RenderCommand>& commands) override;
    bool IsInitialized() const override { return m_isInitialized; }
    RendererAPI GetAPI() const override { return RendererAPI::Vulkan; }
    void SetClearColor(float r, float g, float b, float a) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    
    // Configuration
    const Config& GetConfig() const { return m_config; }
    void UpdateConfig(const Config& config);
    
    // Error handling
    const std::string& GetLastError() const { return m_lastError; }
    
    // Frame management
    float GetFrameTime() const { return m_frameTime; }
    
    // Kamera yönetimi
    void SetCamera(Camera* camera) { m_camera = camera; }
    Camera* GetCamera() const { return m_camera; }
    
    // Public rendering methods
    void RecordCommands(uint32_t frameIndex); // Sadece komut kaydı yapar, submit/present yapmaz
    void RecordCommands(uint32_t frameIndex, const ECSSubsystem::RenderPacket& renderPacket); // ECS verisi ile komut kaydı yapar
    
    // Dynamic rendering methods
    bool InitializeDynamicRendering();
    void BeginDynamicRendering(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void EndDynamicRendering(VkCommandBuffer commandBuffer);
    void RecordDynamicRenderingCommands(uint32_t frameIndex, uint32_t imageIndex);
    
    // Descriptor management
    void UpdateDescriptors(uint32_t frameIndex, const std::shared_ptr<VulkanTexture>& texture);
    
    // Asset-based rendering methods
    void RenderModelWithAssetHandles(VkCommandBuffer commandBuffer, uint32_t frameIndex, 
                                   const ECSSubsystem::RenderPacket::RenderItem& renderItem, 
                                   const std::shared_ptr<Model>& model);
    void RenderPlaceholder(VkCommandBuffer commandBuffer, uint32_t frameIndex, const glm::mat4& transform);

private:
    // Core initialization
    bool InitializeRenderingComponents();
    void ShutdownRenderingComponents();
    
    // Pipeline management
    bool InitializePipeline();
    
    // Rendering
    void RecordCommandBufferWithECS(uint32_t imageIndex, uint32_t frameIndex, const ECSSubsystem::RenderPacket& renderPacket);
    void UpdateUniformBuffer(uint32_t frameIndex);
    void UpdateUniformBufferWithECS(uint32_t frameIndex, const glm::mat4& ecsTransform);
    
    // Error handling
    void HandleVulkanError(VkResult result, const std::string& operation);
    
    // Member variables
    Config m_config;
    
    // Core components (injected from outside)
    GraphicsDevice* m_graphicsDevice = nullptr;
    Engine* m_owner = nullptr;
    
    // Rendering components
    std::unique_ptr<VulkanPipeline> m_pipeline;
    
    // Frame management
    float m_frameTime = 0.0f;
    
    // Render state
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    VkViewport m_currentViewport = {};
    VkRect2D m_currentScissor = {};
    
    // State management
    bool m_isInitialized = false;
    bool m_isFrameStarted = false;
    
    // Kamera
    Camera* m_camera = nullptr;
    
    // Asset Management
    AssetManager* m_assetManager = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Model>> m_modelCache;
    std::unordered_map<std::string, std::shared_ptr<VulkanTexture>> m_textureCache;
    
    // Fallback texture for missing assets (cached to avoid recreation every frame)
    std::shared_ptr<VulkanTexture> m_fallbackTexture;

    // Error handling
    std::string m_lastError;
    
    // Private helper methods
    std::shared_ptr<VulkanTexture> CreateFallbackTexture();
};

} // namespace AstralEngine
