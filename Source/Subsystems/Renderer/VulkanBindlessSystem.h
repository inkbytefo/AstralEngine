#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Core/Logger.h"

namespace AstralEngine {

class GraphicsDevice;

/**
 * @class VulkanBindlessSystem
 * @brief Manages a global bindless descriptor set for textures and buffers.
 */
class VulkanBindlessSystem {
public:
    static const uint32_t MAX_BINDLESS_RESOURCES = 10000;
    static const uint32_t TEXTURE_BINDING = 0;
    static const uint32_t STORAGE_BUFFER_BINDING = 1;
    static const uint32_t STORAGE_IMAGE_BINDING = 2;

    VulkanBindlessSystem() = default;
    ~VulkanBindlessSystem() { Shutdown(); }

    bool Initialize(GraphicsDevice* device);
    void Shutdown();

    uint32_t RegisterTexture(VkImageView imageView, VkSampler sampler);
    uint32_t RegisterStorageBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
    uint32_t RegisterStorageImage(VkImageView imageView);

    VkDescriptorSetLayout GetLayout() const { return m_layout; }
    VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }

private:
    GraphicsDevice* m_device = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    uint32_t m_textureCount = 0;
    uint32_t m_bufferCount = 0;
    uint32_t m_storageImageCount = 0;
};

} // namespace AstralEngine
