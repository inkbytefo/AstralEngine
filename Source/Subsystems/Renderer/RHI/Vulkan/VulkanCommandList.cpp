#include "VulkanCommandList.h"
#include "VulkanDevice.h"
#include "VulkanResources.h"
#include <stdexcept>

namespace AstralEngine {

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
    // Legacy RenderPass path
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_device->GetSwapchainRenderPass();
    renderPassInfo.framebuffer = m_device->GetFramebuffer(m_device->GetCurrentImageIndex());
    renderPassInfo.renderArea.offset = { renderArea.offset.x, renderArea.offset.y };
    renderPassInfo.renderArea.extent = { renderArea.extent.width, renderArea.extent.height };

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandList::EndRendering() {
    vkCmdEndRenderPass(m_commandBuffer);
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
