#pragma once

#include "IRenderer.h"
#include "Core/Engine.h"
#include "Camera.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Material/Material.h"
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

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct ResolvedRenderItem {
        glm::mat4 transform;
        std::shared_ptr<VulkanMesh> mesh;
        std::shared_ptr<Material> material;
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
    
    const Config& GetConfig() const { return m_config; }
    void UpdateConfig(const Config& config);
    
    const std::string& GetLastError() const { return m_lastError; }
    
    float GetFrameTime() const { return m_frameTime; }
    
    void SetCamera(Camera* camera) { m_camera = camera; }
    Camera* GetCamera() const { return m_camera; }
    
    void RecordCommands(uint32_t frameIndex, const std::map<VkPipeline, std::vector<ResolvedRenderItem>>& renderQueue);
    
    bool InitializeDynamicRendering();
    void BeginDynamicRendering(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void EndDynamicRendering(VkCommandBuffer commandBuffer);
    void RecordDynamicRenderingCommands(uint32_t frameIndex, uint32_t imageIndex);
    
    void UpdateDescriptors(uint32_t frameIndex, const std::shared_ptr<VulkanTexture>& texture);
    
private:
    bool InitializeRenderingComponents();
    void ShutdownRenderingComponents();
    
    bool InitializePipeline();
    
    void UpdateUniformBuffer(uint32_t frameIndex);
    void UpdateUniformBufferWithECS(uint32_t frameIndex, const glm::mat4& ecsTransform);
    
    void HandleVulkanError(VkResult result, const std::string& operation);
    
    Config m_config;
    
    GraphicsDevice* m_graphicsDevice = nullptr;
    Engine* m_owner = nullptr;
    
    float m_frameTime = 0.0f;
    
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    VkViewport m_currentViewport = {};
    VkRect2D m_currentScissor = {};
    
    bool m_isInitialized = false;
    bool m_isFrameStarted = false;
    
    Camera* m_camera = nullptr;
    
    std::shared_ptr<VulkanTexture> m_fallbackTexture;

    std::string m_lastError;
    
    std::shared_ptr<VulkanTexture> CreateFallbackTexture();
    void UpdatePushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const glm::mat4& transform);
};

} // namespace AstralEngine
