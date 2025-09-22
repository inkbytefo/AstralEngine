
#pragma once

#include "IRenderPass.h"
#include "../RendererTypes.h"
#include <memory>
#include <map>
#include <vector>

// Forward declarations
namespace AstralEngine {
    class VulkanFramebuffer;
    class VulkanTexture;
    class VulkanPipeline;
    class Material;
}

namespace AstralEngine {

/**
 * @class GBufferPass
 * @brief Manages the G-Buffer generation pass.
 *
 * This pass is responsible for rendering all scene geometry into a set of
 * offscreen textures (the G-Buffer), which includes albedo, normals,
 * PBR properties (metallic, roughness, AO), and depth.
 */
class GBufferPass final : public IRenderPass {
public:
    GBufferPass();
    ~GBufferPass() override;

    // Inherited from IRenderPass
    bool Initialize(RenderSubsystem* owner) override;
    void Shutdown() override;
    void Record(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;
    void OnResize(uint32_t width, uint32_t height) override;
    const char* GetName() const override { return "GBufferPass"; }

    // G-Buffer specific getters
    VulkanTexture* GetAlbedoTexture() const { return m_gBufferAlbedo.get(); }
    VulkanTexture* GetNormalTexture() const { return m_gBufferNormal.get(); }
    VulkanTexture* GetPBRTexture() const { return m_gBufferPBR.get(); }
    VulkanTexture* GetDepthTexture() const { return m_gBufferDepth.get(); }
    VkRenderPass GetVkRenderPass() const { return m_renderPass; }

private:
    void CreateRenderPass();
    void CreateFramebuffer(uint32_t width, uint32_t height);
    void CleanupFramebuffer();

    // Pipeline cache management
    std::shared_ptr<VulkanPipeline> GetOrCreatePipeline(const Material& material);
    VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(const Material& material);
    VkPipelineLayout GetOrCreatePipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

    RenderSubsystem* m_owner = nullptr;
    GraphicsDevice* m_graphicsDevice = nullptr;

    // G-Buffer resources
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<VulkanFramebuffer> m_framebuffer;
    std::unique_ptr<VulkanTexture> m_gBufferAlbedo;
    std::unique_ptr<VulkanTexture> m_gBufferNormal;
    std::unique_ptr<VulkanTexture> m_gBufferPBR;
    std::unique_ptr<VulkanTexture> m_gBufferDepth;

    // Instance buffer for drawing multiple objects efficiently
    std::vector<std::unique_ptr<class VulkanBuffer>> m_instanceBuffers;
    void* m_instanceBufferMapped = nullptr;
    uint32_t m_instanceBufferOffset = 0;
    static const uint32_t INSTANCE_BUFFER_SIZE = 1024 * 1024; // 1MB

    // Pipeline and layout caches, moved from VulkanRenderer
    std::map<size_t, std::shared_ptr<VulkanPipeline>> m_pipelineCache;
    std::unordered_map<size_t, VkDescriptorSetLayout> m_descriptorSetLayoutCache;
    std::unordered_map<size_t, VkPipelineLayout> m_pipelineLayoutCache;
};

} // namespace AstralEngine
