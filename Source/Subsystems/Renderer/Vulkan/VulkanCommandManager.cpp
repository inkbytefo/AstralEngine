#include "VulkanCommandManager.h"
#include "../GraphicsDevice.h"

namespace AstralEngine {

VulkanCommandManager::VulkanCommandManager(GraphicsDevice* device, uint32_t frameCount)
    : m_device(device), m_frameCount(frameCount) {
    Logger::Debug("VulkanCommandManager", "VulkanCommandManager created");
}

VulkanCommandManager::~VulkanCommandManager() {
    Logger::Debug("VulkanCommandManager", "VulkanCommandManager destroyed");
}

void VulkanCommandManager::Initialize() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_device->GetGraphicsQueueFamily();

    if (vkCreateCommandPool(m_device->GetDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    m_commandBuffers.resize(m_frameCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    if (vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    Logger::Info("VulkanCommandManager", "Command pool and buffers initialized successfully.");
}

void VulkanCommandManager::Shutdown() {
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device->GetDevice(), m_commandPool, nullptr);
    }
    Logger::Info("VulkanCommandManager", "Command pool shut down successfully.");
}

VkCommandBuffer VulkanCommandManager::GetCurrentCommandBuffer() const {
    // This is a simplified approach. In a real engine, the frame index
    // would be managed by the main render loop's synchronization logic.
    return m_commandBuffers[m_currentFrameIndex];
}

void VulkanCommandManager::ResetCurrentCommandBuffer() {
    vkResetCommandBuffer(GetCurrentCommandBuffer(), 0);
}

}
