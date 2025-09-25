
#pragma once

#include "IRenderPass.h"
#include "../RendererTypes.h"
#include "../../Asset/AssetHandle.h"
#include "../../../Core/Logger.h"
#include "../Core/VulkanDevice.h"
#include "../Buffers/VulkanBuffer.h"
#include "../Commands/VulkanPipeline.h"
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>

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
 * PBR properties (metallic, roughness, AO), and depth. The G-Buffer is
 * essential for deferred rendering pipelines and provides the necessary
 * information for lighting calculations and post-processing effects.
 *
 * Features:
 * - Multi-render target (MRT) rendering for G-Buffer generation
 * - Pipeline caching for efficient material rendering
 * - Instance buffer management for batched rendering
 * - Comprehensive error handling and validation
 * - Debug and monitoring utilities
 */
class GBufferPass final : public IRenderPass {
public:
    /**
     * @brief Constructs a new GBufferPass instance
     */
    GBufferPass();
    
    /**
     * @brief Destroys the GBufferPass and cleans up resources
     */
    ~GBufferPass() override;

    // Inherited from IRenderPass
    /**
     * @brief Initializes the G-Buffer pass with the given render subsystem
     * @param owner Pointer to the render subsystem that owns this pass
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize(RenderSubsystem* owner) override;
    
    /**
     * @brief Shuts down the G-Buffer pass and releases all resources
     */
    void Shutdown() override;
    
    /**
     * @brief Records rendering commands for the G-Buffer pass
     * @param commandBuffer Vulkan command buffer to record commands to
     * @param frameIndex Current frame index for multi-buffered rendering
     */
    void Record(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;
    
    /**
     * @brief Handles resize events by recreating framebuffer and textures
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void OnResize(uint32_t width, uint32_t height) override;
    
    /**
     * @brief Gets the name of this render pass
     * @return Const char pointer to the pass name
     */
    const char* GetName() const override { return "GBufferPass"; }

    // G-Buffer specific getters
    /**
     * @brief Gets the albedo texture from the G-Buffer
     * @return Pointer to the albedo texture, or nullptr if not initialized
     */
    VulkanTexture* GetAlbedoTexture() const { return m_gBufferAlbedo.get(); }
    
    /**
     * @brief Gets the normal texture from the G-Buffer
     * @return Pointer to the normal texture, or nullptr if not initialized
     */
    VulkanTexture* GetNormalTexture() const { return m_gBufferNormal.get(); }
    
    /**
     * @brief Gets the PBR texture from the G-Buffer
     * @return Pointer to the PBR texture, or nullptr if not initialized
     */
    VulkanTexture* GetPBRTexture() const { return m_gBufferPBR.get(); }
    
    /**
     * @brief Gets the depth texture from the G-Buffer
     * @return Pointer to the depth texture, or nullptr if not initialized
     */
    VulkanTexture* GetDepthTexture() const { return m_gBufferDepth.get(); }
    
    /**
     * @brief Gets the Vulkan render pass handle
     * @return VkRenderPass handle, or VK_NULL_HANDLE if not initialized
     */
    VkRenderPass GetVkRenderPass() const { return m_renderPass; }

    // Error handling and validation methods
    /**
     * @brief Validates the initialization state of the G-Buffer pass
     * @return true if properly initialized, false otherwise
     */
    bool ValidateInitialization() const;
    
    /**
     * @brief Validates the current render state before recording commands
     * @return true if render state is valid, false otherwise
     */
    bool ValidateRenderState() const;
    
    /**
     * @brief Gets the last error message
     * @return Const reference to the last error string
     */
    const std::string& GetLastError() const { return m_lastError; }
    
    /**
     * @brief Clears the last error message
     */
    void ClearError() { m_lastError.clear(); }

    // VulkanDevice access for internal operations
    /**
     * @brief Gets the VulkanDevice for internal operations
     * @return Pointer to VulkanDevice, or nullptr if not available
     */
    VulkanDevice* GetVulkanDevice() const { return m_graphicsDevice ? m_graphicsDevice->GetVulkanDevice() : nullptr; }

    // Debug and monitoring utilities
    /**
     * @brief Logs information about the pipeline cache for debugging
     */
    void LogPipelineCacheInfo() const;
    
    /**
     * @brief Logs information about descriptor set layouts for debugging
     */
    void LogDescriptorSetLayoutInfo() const;
    
    /**
     * @brief Gets the current size of the pipeline cache
     * @return Number of cached pipelines
     */
    uint32_t GetPipelineCacheSize() const { return static_cast<uint32_t>(m_pipelineCache.size()); }
    
    /**
     * @brief Gets the current size of the descriptor set layout cache
     * @return Number of cached descriptor set layouts
     */
    uint32_t GetDescriptorSetLayoutCacheSize() const { return static_cast<uint32_t>(m_descriptorSetLayoutCache.size()); }
    
    /**
     * @brief Gets the current size of the pipeline layout cache
     * @return Number of cached pipeline layouts
     */
    uint32_t GetPipelineLayoutCacheSize() const { return static_cast<uint32_t>(m_pipelineLayoutCache.size()); }

private:
    /**
     * @brief Creates the Vulkan render pass for G-Buffer generation
     */
    void CreateRenderPass();
    
    /**
     * @brief Creates the framebuffer and G-Buffer textures
     * @param width Width of the framebuffer in pixels
     * @param height Height of the framebuffer in pixels
     */
    void CreateFramebuffer(uint32_t width, uint32_t height);
    
    /**
     * @brief Cleans up framebuffer and G-Buffer texture resources
     */
    void CleanupFramebuffer();

    // Pipeline cache management
    /**
     * @brief Gets or creates a pipeline for the given material
     * @param material Reference to the material to create pipeline for
     * @return Shared pointer to the Vulkan pipeline, or nullptr if creation failed
     */
    std::shared_ptr<VulkanPipeline> GetOrCreatePipeline(const Material& material);
    
    /**
     * @brief Gets or creates a descriptor set layout for the given material
     * @param material Reference to the material to create layout for
     * @return VkDescriptorSetLayout handle, or VK_NULL_HANDLE if creation failed
     */
    VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(const Material& material);
    
    /**
     * @brief Gets or creates a pipeline layout for the given descriptor set layout
     * @param descriptorSetLayout Descriptor set layout to create pipeline layout for
     * @return VkPipelineLayout handle, or VK_NULL_HANDLE if creation failed
     */
    VkPipelineLayout GetOrCreatePipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Aligns an offset to the specified alignment
     * @param offset The offset to align
     * @param alignment The alignment to use
     * @return Aligned offset
     */
    uint32_t AlignOffset(uint32_t offset, VkDeviceSize alignment) const {
        if (alignment <= 1) return offset;
        return static_cast<uint32_t>((offset + alignment - 1) & ~(alignment - 1));
    }

    RenderSubsystem* m_owner = nullptr;
    GraphicsDevice* m_graphicsDevice = nullptr;
    
    // Error handling
    std::string m_lastError;

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
    VkDeviceSize m_vertexBufferOffsetAlignment = 0; // Alignment for vertex buffer offsets
    static const uint32_t INSTANCE_BUFFER_SIZE = 1024 * 1024; // 1MB

    // Pipeline and layout caches, moved from VulkanRenderer
    std::map<size_t, std::shared_ptr<VulkanPipeline>> m_pipelineCache;
    std::unordered_map<size_t, VkDescriptorSetLayout> m_descriptorSetLayoutCache;
    std::unordered_map<size_t, VkPipelineLayout> m_pipelineLayoutCache;
};

} // namespace AstralEngine
