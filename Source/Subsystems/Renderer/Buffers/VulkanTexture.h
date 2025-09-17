#pragma once

#include "../Core/VulkanDevice.h"
#include <string>

namespace AstralEngine {

class VulkanTexture {
public:
    VulkanTexture();
    ~VulkanTexture();

    bool Initialize(VulkanDevice* device, const std::string& texturePath);
    bool InitializeFromData(VulkanDevice* device, const void* data, uint32_t width, uint32_t height, VkFormat format);
    void Shutdown();

    VkImageView GetImageView() const { return m_textureImageView; }
    VkSampler GetSampler() const { return m_textureSampler; }
    bool IsInitialized() const { return m_isInitialized; }
    const std::string& GetLastError() const { return m_lastError; }

private:
    void CreateTextureImage(const std::string& path);
    void CreateTextureImageFromData(const void* data, uint32_t width, uint32_t height, VkFormat format);
    void CreateTextureImageView();
    void CreateTextureSampler();
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    VulkanDevice* m_device = nullptr;
    VkImage m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureImageMemory = VK_NULL_HANDLE;
    VkImageView m_textureImageView = VK_NULL_HANDLE;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    
    int m_texWidth = 0, m_texHeight = 0, m_texChannels = 0;
    bool m_isInitialized = false;
    std::string m_lastError;
    void SetError(const std::string& error);
};

} // namespace AstralEngine
