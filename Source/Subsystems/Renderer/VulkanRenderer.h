#pragma once

#include "IRenderer.h"
#include "Core/Engine.h"
#include "Camera.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Material/Material.h"
#include "RenderSubsystem.h" // For MeshMaterialKey
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <glm/glm.hpp>

namespace AstralEngine {

class GraphicsDevice;
class VulkanShader;
class VulkanPipeline;
class VulkanBuffer;
class VulkanMesh;
class VulkanTexture;
class RenderSubsystem;
class VulkanFramebuffer; // Still used for custom texture targets, but not as VkFramebuffer

class VulkanRenderer : public IRenderer {
public:
    struct ResolvedRenderItem {
        glm::mat4 transform;
        std::shared_ptr<VulkanMesh> mesh;
        std::shared_ptr<Material> material;
    };

    struct PushConstants {
        glm::mat4 model;               // 64
        glm::vec3 baseColorFactor;     // 12
        float metallicFactor;          // 4
        float roughnessFactor;         // 4
        float aoFactor;                // 4
        glm::vec3 emissiveFactor;      // 12
        uint32_t albedoIndex;          // 4
        uint32_t normalIndex;          // 4
        uint32_t metallicIndex;        // 4
        uint32_t roughnessIndex;       // 4
        uint32_t aoIndex;              // 4
        uint32_t emissiveIndex;        // 4
        // Total: 64 + 12 + 4 + 4 + 4 + 12 + 24 = 124 bytes
    };

    VulkanRenderer();
    virtual ~VulkanRenderer();

    bool Initialize(GraphicsDevice* device, void* owner = nullptr) override;
    void Shutdown() override;

    void SetCamera(Camera* camera) { m_camera = camera; }
    
    // Modern Command recording with Dynamic Rendering
    void RecordShadowPassCommands(VulkanTexture* depthTarget, const glm::mat4& lightSpaceMatrix, const std::vector<ResolvedRenderItem>& renderItems);
    void RecordGBufferCommands(uint32_t frameIndex, const std::vector<VulkanTexture*>& colorTargets, VulkanTexture* depthTarget, const std::map<MeshMaterialKey, std::vector<glm::mat4>>& renderQueue);
    void RecordLightingCommands(uint32_t frameIndex, VulkanTexture* outputTarget, const std::vector<VulkanTexture*>& gbufferInputs, VulkanTexture* depthInput);
    
    void ResetInstanceBuffer();

    // Interface implementations
    void BeginFrame() override {};
    void EndFrame() override;
    void Present() override {};
    void Submit(const RenderCommand& command) override {};
    void SubmitCommands(const std::vector<RenderCommand>& commands) override {};
    bool IsInitialized() const override { return m_isInitialized; };
    RendererAPI GetAPI() const override { return RendererAPI::Vulkan; };
    void SetClearColor(float r, float g, float b, float a) override {};
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {};
    
    // G-Buffer texture accessors for post-processing or lighting
    VulkanTexture* GetGBufferAlbedo() const;
    VulkanTexture* GetGBufferNormal() const;
    VulkanTexture* GetGBufferPBR() const;
    VulkanTexture* GetGBufferDepth() const;

private:
    bool InitializeRenderingComponents();
    void ShutdownRenderingComponents();
    
    // Pipeline management
    std::shared_ptr<VulkanPipeline> GetOrCreatePipeline(const Material& material, bool isShadowPass = false);
    VkDescriptorSetLayout GetGlobalDescriptorSetLayout();
    VkPipelineLayout GetGlobalPipelineLayout();

    GraphicsDevice* m_graphicsDevice = nullptr;
    RenderSubsystem* m_renderSubsystem = nullptr;
    Engine* m_owner = nullptr;
    
    bool m_isInitialized = false;
    Camera* m_camera = nullptr;
    
    // Pipelines
    std::shared_ptr<VulkanPipeline> m_lightingPipeline;
    std::shared_ptr<VulkanPipeline> m_shadowPipeline;
    
    // Caches
    std::map<size_t, std::shared_ptr<VulkanPipeline>> m_pipelineCache;
    VkDescriptorSetLayout m_globalDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_globalPipelineLayout = VK_NULL_HANDLE;
    
    // Instance buffers
    std::vector<std::unique_ptr<VulkanBuffer>> m_instanceBuffers;
    void* m_instanceBufferMapped = nullptr;
    uint32_t m_instanceBufferOffset = 0;
    static const uint32_t INSTANCE_BUFFER_SIZE = 1024 * 1024;
};

} // namespace AstralEngine
