#include "VulkanCommandList.h"
#include "VulkanDevice.h"
#include "VulkanResources.h"
#include <stdexcept>
#include <array>
#include <vector>

namespace AstralEngine {

// Member function for layout transition using Synchronization 2
void VulkanCommandList::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, bool isDepth) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    
    if (isDepth) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Define source and destination stages and access masks based on layout transition
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.dstAccessMask = 0;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    } else {
        // Fallback for other transitions
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    auto func = (PFN_vkCmdPipelineBarrier2)vkGetDeviceProcAddr(m_device->GetVkDevice(), "vkCmdPipelineBarrier2");
    if (func) {
        func(m_commandBuffer, &dependencyInfo);
    } else {
        // Fallback to legacy barrier if somehow sync2 is not available (though we checked for it)
        VkImageMemoryBarrier legacyBarrier{};
        legacyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        legacyBarrier.oldLayout = barrier.oldLayout;
        legacyBarrier.newLayout = barrier.newLayout;
        legacyBarrier.image = barrier.image;
        legacyBarrier.subresourceRange = barrier.subresourceRange;
        legacyBarrier.srcAccessMask = (VkAccessFlags)barrier.srcAccessMask;
        legacyBarrier.dstAccessMask = (VkAccessFlags)barrier.dstAccessMask;

        vkCmdPipelineBarrier(
            m_commandBuffer,
            (VkPipelineStageFlags)barrier.srcStageMask,
            (VkPipelineStageFlags)barrier.dstStageMask,
            0, 0, nullptr, 0, nullptr, 1, &legacyBarrier);
    }
}

void VulkanCommandList::TransitionImageLayout(VulkanTexture* texture, VkImageLayout newLayout, bool isDepth) {
    if (texture->GetLayout() == newLayout) return;
    
    TransitionImageLayout(texture->GetImage(), texture->GetLayout(), newLayout, isDepth);
    texture->SetLayout(newLayout);
}

VulkanCommandList::VulkanCommandList(VulkanDevice* device, VkCommandPool pool)
    : m_device(device), m_pool(pool) {
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device->GetVkDevice(), &allocInfo, &m_commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

VulkanCommandList::~VulkanCommandList() {
    // Do NOT free the command buffer here.
    // The command pool is reset at the beginning of each frame (in VulkanDevice::BeginFrame),
    // which effectively invalidates/frees all command buffers allocated from it.
    // Explicitly freeing it here causes a validation error because the buffer is still in use (pending execution)
    // when this destructor is called at the end of RenderSubsystem::OnUpdate.
    
    // vkFreeCommandBuffers(m_device->GetVkDevice(), m_pool, 1, &m_commandBuffer);
}

void VulkanCommandList::Begin() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
}

void VulkanCommandList::End() {
    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void VulkanCommandList::BeginRendering(const std::vector<IRHITexture*>& colorAttachments, IRHITexture* depthAttachment, const RHIRect2D& renderArea) {
    m_activeColorAttachments.clear();

    std::vector<VkRenderingAttachmentInfo> colorInfos;
    colorInfos.reserve(colorAttachments.size());

    for (auto* texture : colorAttachments) {
        auto* vkTexture = static_cast<VulkanTexture*>(texture);
        m_activeColorAttachments.push_back(vkTexture);
        
        // Transition to COLOR_ATTACHMENT_OPTIMAL
        TransitionImageLayout(vkTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = vkTexture->GetImageView();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
        colorInfos.push_back(colorAttachment);
    }

    VkRenderingAttachmentInfo depthInfo{};
    bool hasDepth = (depthAttachment != nullptr);
    if (hasDepth) {
        auto* vkTexture = static_cast<VulkanTexture*>(depthAttachment);
        
        // Transition to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        TransitionImageLayout(vkTexture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true);

        depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthInfo.imageView = vkTexture->GetImageView();
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthInfo.clearValue.depthStencil = { 1.0f, 0 };
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {renderArea.offset.x, renderArea.offset.y}, {renderArea.extent.width, renderArea.extent.height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorInfos.size());
    renderingInfo.pColorAttachments = colorInfos.data();
    renderingInfo.pDepthAttachment = hasDepth ? &depthInfo : nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    auto func = (PFN_vkCmdBeginRendering)vkGetDeviceProcAddr(m_device->GetVkDevice(), "vkCmdBeginRendering");
    if (func) {
        func(m_commandBuffer, &renderingInfo);
    }
}

void VulkanCommandList::EndRendering() {
    auto func = (PFN_vkCmdEndRendering)vkGetDeviceProcAddr(m_device->GetVkDevice(), "vkCmdEndRendering");
    if (func) {
        func(m_commandBuffer);
    }

    // Transition color attachments to their optimal post-render layout
    for (auto* vkTexture : m_activeColorAttachments) {
        // If it's a swapchain image, transition to PRESENT_SRC_KHR.
        // If it's an offscreen texture (like the editor viewport), transition to SHADER_READ_ONLY_OPTIMAL.
        VkImageLayout finalLayout = vkTexture->IsSwapchainTexture() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        TransitionImageLayout(vkTexture, finalLayout);
    }
    m_activeColorAttachments.clear();
}

void VulkanCommandList::BindPipeline(IRHIPipeline* pipeline) {
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VulkanPipeline*>(pipeline)->GetPipeline());
}

void VulkanCommandList::BindDescriptorSet(IRHIPipeline* pipeline, IRHIDescriptorSet* descriptorSet, uint32_t setIndex) {
    VkDescriptorSet vkSet = static_cast<VulkanDescriptorSet*>(descriptorSet)->GetVkDescriptorSet();
    VkPipelineLayout layout = static_cast<VulkanPipeline*>(pipeline)->GetLayout();
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, setIndex, 1, &vkSet, 0, nullptr);
}

void VulkanCommandList::SetViewport(const RHIViewport& viewport) {
    VkViewport vkViewport{};
    vkViewport.x = viewport.x;
    vkViewport.y = viewport.y;
    vkViewport.width = viewport.width;
    vkViewport.height = viewport.height;
    vkViewport.minDepth = viewport.minDepth;
    vkViewport.maxDepth = viewport.maxDepth;
    vkCmdSetViewport(m_commandBuffer, 0, 1, &vkViewport);
}

void VulkanCommandList::SetScissor(const RHIRect2D& scissor) {
    VkRect2D vkScissor{};
    vkScissor.offset = { scissor.offset.x, scissor.offset.y };
    vkScissor.extent = { scissor.extent.width, scissor.extent.height };
    vkCmdSetScissor(m_commandBuffer, 0, 1, &vkScissor);
}

void VulkanCommandList::BindVertexBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset) {
    VkBuffer vkBuffer = static_cast<VulkanBuffer*>(buffer)->GetBuffer();
    VkDeviceSize offsets[] = { offset };
    vkCmdBindVertexBuffers(m_commandBuffer, binding, 1, &vkBuffer, offsets);
}

void VulkanCommandList::BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, bool is32Bit) {
    vkCmdBindIndexBuffer(m_commandBuffer, static_cast<VulkanBuffer*>(buffer)->GetBuffer(), offset, is32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanCommandList::PushConstants(IRHIPipeline* pipeline, RHIShaderStage stage, uint32_t offset, uint32_t size, const void* data) {
    VkShaderStageFlags stageFlags = 0;
    if (static_cast<int>(stage) & static_cast<int>(RHIShaderStage::Vertex)) stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<int>(stage) & static_cast<int>(RHIShaderStage::Fragment)) stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

    vkCmdPushConstants(m_commandBuffer, static_cast<VulkanPipeline*>(pipeline)->GetLayout(), stageFlags, offset, size, data);
}

} // namespace AstralEngine
