#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace AstralEngine {

class GraphicsDevice;

class VulkanCommandManager {
public:
    VulkanCommandManager(GraphicsDevice* device, uint32_t frameCount);
    ~VulkanCommandManager();

    void Initialize();
    void Shutdown();

    VkCommandBuffer GetCurrentCommandBuffer() const;
    void ResetCurrentCommandBuffer();

private:
    GraphicsDevice* m_device; // Non-owning
    uint32_t m_frameCount;
    uint32_t m_currentFrameIndex = 0;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

}
