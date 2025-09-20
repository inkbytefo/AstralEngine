#pragma once
#include "Core/VulkanDevice.h"
#include <functional>
#include <queue>
#include <mutex>

namespace AstralEngine {

class VulkanTransferManager {
public:
    VulkanTransferManager(VulkanDevice* device);
    ~VulkanTransferManager();

    void Initialize();
    void Shutdown();

    // Queues a copy operation from a staging buffer to a final buffer/image
    void QueueTransfer(VkBuffer stagingBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void QueueTransfer(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height);

    // Submits all queued transfers for the frame
    void SubmitTransfers();

private:
    VulkanDevice* m_device;
    VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_transferCommandBuffer = VK_NULL_HANDLE;
    VkFence m_transferFence = VK_NULL_HANDLE;
    
    std::mutex m_queueMutex;
    // In a real-world scenario, you'd queue more structured request objects
    std::vector<std::function<void(VkCommandBuffer)>> m_transferQueue;
};

} // namespace AstralEngine