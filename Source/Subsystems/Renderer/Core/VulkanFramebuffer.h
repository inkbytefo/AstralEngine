#pragma once

#include "../../../Core/Logger.h"
#include "VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace AstralEngine {

/**
 * @class VulkanFramebuffer
 * @brief Vulkan framebuffer yönetimi için temel sınıf
 * 
 * Bu sınıf, jenerik ve yeniden kullanılabilir framebuffer'lar oluşturmak için kullanılır.
 * Render-to-texture yeteneklerini (gölgeler, post-processing, yansımalar vb.) destekler.
 * RAII pattern'ine uygun olarak tasarlanmıştır ve açık Shutdown() metodu gerektirir.
 */
class VulkanFramebuffer {
public:
    /**
     * @brief Framebuffer yapılandırması için ayarlar
     */
    struct Config {
        VulkanDevice* device = nullptr;              ///< Vulkan cihazı
        VkRenderPass renderPass = VK_NULL_HANDLE;    ///< Render pass
        std::vector<VkImageView> attachments;        ///< Attachment görüntü görünümleri
        uint32_t width = 0;                          ///< Framebuffer genişliği
        uint32_t height = 0;                         ///< Framebuffer yüksekliği
        uint32_t layers = 1;                         ///< Katman sayısı (default: 1)
        std::string name = "UnnamedFramebuffer";     ///< Framebuffer adı (debug için)
    };

    VulkanFramebuffer();
    ~VulkanFramebuffer();

    // Lifecycle management
    bool Initialize(const Config& config);
    void Shutdown();

    // Getter methods
    VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetLayers() const { return m_layers; }
    const std::vector<VkImageView>& GetAttachments() const { return m_attachments; }
    
    // State management
    bool IsInitialized() const { return m_isInitialized; }
    
    // Error handling
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Helper methods
    void HandleVulkanError(VkResult result, const std::string& operation);
    void SetError(const std::string& error);

    // Member variables
    Config m_config;
    std::string m_lastError;
    
    // Vulkan resources
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    
    // Framebuffer properties
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_layers = 1;
    std::vector<VkImageView> m_attachments;
    
    // State management
    bool m_isInitialized = false;
};

} // namespace AstralEngine