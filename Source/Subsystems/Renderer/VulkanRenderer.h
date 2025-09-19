#pragma once

#include "IRenderer.h"
#include "Core/Engine.h"
#include "Camera.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Material/Material.h"
#include "Core/VulkanFramebuffer.h"
#include "RenderSubsystem.h" // For MeshMaterialKey
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <glm/glm.hpp>

// Forward declarations
namespace AstralEngine {
class GraphicsDevice;
class VulkanShader;
class VulkanPipeline;
class VulkanBuffer;
class VulkanMesh;
class VulkanTexture;
class RenderSubsystem;

class VulkanRenderer : public IRenderer {
public>
    struct ResolvedRenderItem {
        glm::mat4 transform;
        std::shared_ptr<VulkanMesh> mesh;
        std::shared_ptr<Material> material;
    };

    VulkanRenderer();
    virtual ~VulkanRenderer();

    bool Initialize(GraphicsDevice* device, void* owner = nullptr) override;
    void Shutdown() override;

    void SetCamera(Camera* camera) { m_camera = camera; }
    
    // Command recording for different passes
    void RecordShadowPassCommands(VulkanFramebuffer* shadowFramebuffer, const glm::mat4& lightSpaceMatrix, const std::vector<ResolvedRenderItem>& renderItems);
    void RecordGBufferCommands(uint32_t frameIndex, VulkanFramebuffer* gBuffer, const std::map<MeshMaterialKey, std::vector<glm::mat4>>& renderQueue);
    void RecordLightingCommands(uint32_t frameIndex, VulkanFramebuffer* sceneFramebuffer);
    
    // Instance buffer management
    void ResetInstanceBuffer();

    VkRenderPass GetGBufferRenderPass() const { return m_gBufferRenderPass; }
    VkRenderPass GetLightingRenderPass() const { return m_lightingRenderPass; }
    VkRenderPass GetShadowRenderPass() const { return m_shadowRenderPass; }

    // Unused interface methods are omitted for brevity
    void BeginFrame() override {};
    void EndFrame() override;
    void Present() override {};
    void Submit(const RenderCommand& command) override {};
    void SubmitCommands(const std::vector<RenderCommand>& commands) override {};
    bool IsInitialized() const override { return m_isInitialized; };
    RendererAPI GetAPI() const override { return RendererAPI::Vulkan; };
    void SetClearColor(float r, float g, float b, float a) override {};
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {};

private:
    bool InitializeRenderingComponents();
    void ShutdownRenderingComponents();
    
    void CreateGBufferRenderPass();
    void CreateLightingPass();
    void CreateShadowPass();

    // Pipeline cache management
    std::shared_ptr<VulkanPipeline> GetOrCreatePipeline(const Material& material);

    GraphicsDevice* m_graphicsDevice = nullptr;
    RenderSubsystem* m_renderSubsystem = nullptr;
    Engine* m_owner = nullptr;
    
    bool m_isInitialized = false;
    Camera* m_camera = nullptr;
    
    // Pass Resources
    VkRenderPass m_gBufferRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_lightingRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;

    std::shared_ptr<VulkanPipeline> m_lightingPipeline;
    VkDescriptorSetLayout m_lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingDescriptorSet = VK_NULL_HANDLE;

    std::shared_ptr<VulkanPipeline> m_shadowPipeline;
    
    // Pipeline cache for material-based pipeline management
    std::map<size_t, std::shared_ptr<VulkanPipeline>> m_pipelineCache;
    
    // Frame-ringed instance buffer for performance optimization
    std::vector<std::unique_ptr<VulkanBuffer>> m_instanceBuffers;
    void* m_instanceBufferMapped = nullptr;
    uint32_t m_instanceBufferOffset = 0;
    static const uint32_t INSTANCE_BUFFER_SIZE = 1024 * 1024; // 1MB, adjust as needed
};

} // namespace AstralEngine
