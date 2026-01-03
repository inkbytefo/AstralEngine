#pragma once

#include "../IRHICommandList.h"
#include "../IRHIDescriptor.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace AstralEngine {

class VulkanDevice;
class VulkanTexture;

class VulkanCommandList : public IRHICommandList {
public:
    VulkanCommandList(VulkanDevice* device, VkCommandPool pool);
    ~VulkanCommandList() override;

    void Begin() override;
    void End() override;

    void BeginRendering(const std::vector<IRHITexture*>& colorAttachments, IRHITexture* depthAttachment, const RHIRect2D& renderArea) override;
    void BeginRendering(const std::vector<RHIRenderingAttachment>& colorAttachments, const RHIRenderingAttachment* depthAttachment, const RHIRect2D& renderArea) override;
    void EndRendering() override;

    void BindPipeline(IRHIPipeline* pipeline) override;
    
    void BindDescriptorSet(IRHIPipeline* pipeline, IRHIDescriptorSet* descriptorSet, uint32_t setIndex) override;
    
    void SetViewport(const RHIViewport& viewport) override;
    void SetScissor(const RHIRect2D& scissor) override;

    void BindVertexBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset) override;
    void BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, bool is32Bit) override;

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;

    void PushConstants(IRHIPipeline* pipeline, RHIShaderStage stage, uint32_t offset, uint32_t size, const void* data) override;
    
    // Resource transitions
    void TransitionImageLayout(IRHITexture* texture, int oldLayout, int newLayout) override;

    // Vulkan specific
    void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, 
                               uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                               uint32_t baseArrayLayer = 0, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS,
                               bool isDepth = false);
    
    void TransitionImageLayout(VulkanTexture* texture, VkImageLayout newLayout, 
                               uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                               uint32_t baseArrayLayer = 0, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS,
                               bool isDepth = false, VkImageLayout oldLayout = VK_IMAGE_LAYOUT_MAX_ENUM);

    VkCommandBuffer GetCommandBuffer() const { return m_commandBuffer; }

private:
    VulkanDevice* m_device;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkCommandPool m_pool;

    struct ActiveAttachment {
        VulkanTexture* texture;
        uint32_t mipLevel;
        uint32_t arrayLayer;
    };
    std::vector<ActiveAttachment> m_activeColorAttachments;
    ActiveAttachment m_activeDepthAttachment;
    bool m_hasActiveDepthAttachment = false;
};

} // namespace AstralEngine
