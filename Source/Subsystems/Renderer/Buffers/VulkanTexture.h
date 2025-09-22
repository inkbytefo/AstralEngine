#pragma once

#include "../Core/VulkanDevice.h"
#include "../VulkanMeshManager.h"
#include "../GraphicsDevice.h"
#include <string>

namespace AstralEngine {

/**
 * @class VulkanTexture
 * @brief Modern Vulkan texture management class using centralized transfer system
 *
 * This class has been refactored to use GraphicsDevice* instead of VulkanDevice* directly
 * and removes all manual staging buffer management. The new approach:
 *
 * - Uses GraphicsDevice as the primary interface for all Vulkan operations
 * - Leverages the centralized VulkanTransferManager for texture data uploads
 * - Eliminates manual staging buffer creation and cleanup
 * - Provides a cleaner, more maintainable API
 * - Reduces code duplication and potential memory leaks
 *
 * The GraphicsDevice provides access to:
 * - VulkanDevice through GetVulkanDevice()
 * - VulkanTransferManager through GetTransferManager()
 * - All other Vulkan subsystems (memory manager, synchronization, etc.)
 */
class VulkanTexture {
public:
    /**
     * @brief Texture yapılandırması için ayarlar
     */
    struct Config {
        uint32_t width = 0;                          ///< Texture genişliği
        uint32_t height = 0;                         ///< Texture yüksekliği
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;   ///< Texture formatı
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; ///< Kullanım amaçları
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; ///< Aspect mask
        std::string name = "UnnamedTexture";         ///< Texture adı (debug için)
    };

    VulkanTexture();
    ~VulkanTexture();

    bool Initialize(GraphicsDevice* device, const std::string& texturePath);
    bool Initialize(GraphicsDevice* device, const Config& config);
    bool InitializeFromData(GraphicsDevice* device, const void* data, uint32_t width, uint32_t height, VkFormat format);
    void Shutdown();

    VkImageView GetImageView() const { return m_textureImageView; }
    VkSampler GetSampler() const { return m_textureSampler; }
    bool IsInitialized() const { return m_isInitialized; }
    
    /**
     * @brief Texture'in GPU'da kullanıma hazır olup olmadığını kontrol eder
     *
     * @return true Her zaman true döndürür (artık transfer işlemleri senkronize)
     */
    bool IsReady();
    
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
    void CreateEmptyTexture(const Config& config);
    void CreateTextureImageView();
    void CreateTextureSampler();
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    

    GraphicsDevice* m_graphicsDevice = nullptr;       // GraphicsDevice için pointer
    VulkanDevice* m_device = nullptr;                  // VulkanDevice için pointer (internal use)
    VkImage m_textureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_textureImageMemory = VK_NULL_HANDLE;
    VkImageView m_textureImageView = VK_NULL_HANDLE;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    
    int m_texWidth = 0, m_texHeight = 0, m_texChannels = 0;
    bool m_isInitialized = false;
    std::string m_lastError;
    void SetError(const std::string& error);
    
    // Transfer için üye değişkenler
    GpuResourceState m_state = GpuResourceState::Unloaded; // Texture'in yükleme durumu
    
    // Geçici olarak saklanacak bilgiler
    uint32_t m_width = 0;                             // Texture genişliği
    uint32_t m_height = 0;                            // Texture yüksekliği
    VkFormat m_format = VK_FORMAT_R8G8B8A8_SRGB;      // Texture formatı
};

} // namespace AstralEngine
