#include "VulkanTransferManager.h"
#include "../../../Core/Logger.h"

namespace AstralEngine {

VulkanTransferManager::VulkanTransferManager(VulkanDevice* device)
    : m_device(device) {
    
}

VulkanTransferManager::~VulkanTransferManager() {
    Shutdown();
}

void VulkanTransferManager::Initialize() {
    if (!m_device || !m_device->GetDevice()) {
        Logger::Error("VulkanTransferManager", "Device not initialized");
        return;
    }

    // Create transfer command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_device->GetQueueFamilyIndices().transferFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(m_device->GetDevice(), &poolInfo, nullptr, &m_transferCommandPool) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to create transfer command pool");
        return;
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_transferCommandPool;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, &m_transferCommandBuffer) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to allocate transfer command buffer");
        vkDestroyCommandPool(m_device->GetDevice(), m_transferCommandPool, nullptr);
        m_transferCommandPool = VK_NULL_HANDLE;
        return;
    }

    // Create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(m_device->GetDevice(), &fenceInfo, nullptr, &m_transferFence) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to create transfer fence");
        vkFreeCommandBuffers(m_device->GetDevice(), m_transferCommandPool, 1, &m_transferCommandBuffer);
        vkDestroyCommandPool(m_device->GetDevice(), m_transferCommandPool, nullptr);
        m_transferCommandBuffer = VK_NULL_HANDLE;
        m_transferCommandPool = VK_NULL_HANDLE;
        return;
    }

    Logger::Info("VulkanTransferManager", "Transfer manager initialized successfully");
}

void VulkanTransferManager::Shutdown() {
    if (!m_device || !m_device->GetDevice()) {
        return;
    }

    // Wait for device to finish all operations
    vkDeviceWaitIdle(m_device->GetDevice());

    // Destroy fence
    if (m_transferFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device->GetDevice(), m_transferFence, nullptr);
        m_transferFence = VK_NULL_HANDLE;
    }

    // Command buffer will be destroyed with the pool
    m_transferCommandBuffer = VK_NULL_HANDLE;

    // Destroy command pool
    if (m_transferCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device->GetDevice(), m_transferCommandPool, nullptr);
        m_transferCommandPool = VK_NULL_HANDLE;
    }

    // Clear transfer queue
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_transferQueue.clear();

    Logger::Info("VulkanTransferManager", "Transfer manager shutdown completed");
}

void VulkanTransferManager::QueueTransfer(VkBuffer stagingBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    // Create a lambda function for buffer-to-buffer transfer
    auto transferFunction = [stagingBuffer, dstBuffer, size](VkCommandBuffer commandBuffer) {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);
    };
    
    m_transferQueue.push_back(transferFunction);
}

void VulkanTransferManager::QueueTransfer(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    // Create a lambda function for buffer-to-image transfer
    auto transferFunction = [stagingBuffer, dstImage, width, height](VkCommandBuffer commandBuffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    };
    
    m_transferQueue.push_back(transferFunction);
}

void VulkanTransferManager::SubmitTransfers() {
    if (!m_device || !m_device->GetDevice() || !m_device->GetTransferQueue()) {
        Logger::Error("VulkanTransferManager", "Device or transfer queue not initialized");
        return;
    }

    // Check if queue is empty
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_transferQueue.empty()) {
            return;
        }
    }

    // Wait for fence to ensure previous transfers have completed
    vkWaitForFences(m_device->GetDevice(), 1, &m_transferFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->GetDevice(), 1, &m_transferFence);

    // Reset command buffer
    if (vkResetCommandBuffer(m_transferCommandBuffer, 0) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to reset command buffer");
        return;
    }

    // Begin command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(m_transferCommandBuffer, &beginInfo) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to begin command buffer");
        return;
    }

    // Apply all queued transfer functions
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (auto& transferFunction : m_transferQueue) {
            transferFunction(m_transferCommandBuffer);
        }
    }

    // End command buffer recording
    if (vkEndCommandBuffer(m_transferCommandBuffer) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to end command buffer");
        return;
    }

    // Submit to transfer queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_transferCommandBuffer;

    if (vkQueueSubmit(m_device->GetTransferQueue(), 1, &submitInfo, m_transferFence) != VK_SUCCESS) {
        Logger::Error("VulkanTransferManager", "Failed to submit transfer commands");
        return;
    }

    // Wait for fence to ensure transfers have completed
    vkWaitForFences(m_device->GetDevice(), 1, &m_transferFence, VK_TRUE, UINT64_MAX);
    
    // Reset fence for next use
    vkResetFences(m_device->GetDevice(), 1, &m_transferFence);

    // Clear transfer queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_transferQueue.clear();
    }

    Logger::Debug("VulkanTransferManager", "Transfers submitted successfully");
}

} // namespace AstralEngine