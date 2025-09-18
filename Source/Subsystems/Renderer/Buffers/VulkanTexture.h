#pragma once

#include "../Core/VulkanDevice.h"
#include "../VulkanMeshManager.h"
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
    
    /**
     * @brief Texture'in GPU'da kullanıma hazır olup olmadığını kontrol eder
     *
     * @return true Hazırsa (fence sinyal vermişse)
     * @return false Henüz hazır değilse
     */
    bool IsReady() const;
    
    /**
     * @brief Texture'in yükleme durumunu döndürür
     *
     * @return GpuResourceState Texture'in yükleme durumu
     */
    GpuResourceState GetState() const { return m_state; }
    
    const std::string& GetLastError() const { return m_lastError; }

private:
    void CreateTextureImage(const std::string& path);
    void CreateTextureImageFromData(const void* data, uint32_t width, uint32_t height, VkFormat format);
    void CreateTextureImageView();
    void CreateTextureSampler();
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    
    /**
     * @brief Staging buffer ve fence kaynaklarını temizler
     *
     * Bu metod, asenkron upload işlemi tamamlandığında staging buffer
     * ve fence kaynaklarını temizlemek için kullanılır.
     */
    void CleanupStagingResources();
    
    /**
     * @brief Image initialization'ını tamamlar
     *
     * Bu metod, fence sinyal verdiğinde layout transition'larını tamamlamak için kullanılır.
     */
    void CompleteImageInitialization();

    VulkanDevice* m_device = nullptr;
    VkImage m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureImageMemory = VK_NULL_HANDLE;
    VkImageView m_textureImageView = VK_NULL_HANDLE;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    
    int m_texWidth = 0, m_texHeight = 0, m_texChannels = 0;
    bool m_isInitialized = false;
    std::string m_lastError;
    void SetError(const std::string& error);
    
    // Asenkron upload için üye değişkenler
    VkFence m_uploadFence = VK_NULL_HANDLE;           // Upload işlemini senkronize etmek için fence
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;        // Geçici staging buffer
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;  // Staging buffer için bellek
    GpuResourceState m_state = GpuResourceState::Unloaded; // Texture'in yükleme durumu
    
    // Geçici olarak saklanacak bilgiler
    uint32_t m_width = 0;                             // Texture genişliği
    uint32_t m_height = 0;                            // Texture yüksekliği
    VkFormat m_format = VK_FORMAT_R8G8B8A8_SRGB;      // Texture formatı
};

} // namespace AstralEngine
