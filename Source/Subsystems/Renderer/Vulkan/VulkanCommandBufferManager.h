#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
namespace AstralEngine {
class VulkanDevice;
class VulkanCommandPool;
}

namespace AstralEngine {

/**
 * @struct VulkanCommandBufferManagerConfig
 * @brief Komut buffer yöneticisi yapılandırma parametreleri
 */
struct VulkanCommandBufferManagerConfig {
    uint32_t maxFramesInFlight = 2; // MAX_FRAMES_IN_FLIGHT
    bool enableDebugMarkers = false;
};

/**
 * @class VulkanCommandBufferManager
 * @brief Vulkan komut buffer'larını ve senkronizasyon nesnelerini yönetir
 * 
 * Bu sınıf, komut havuzları, komut buffer'ları ve senkronizasyon
 * nesneleri (semaphores, fences) yönetiminden sorumludur. Multi-frame
 * render mantığını uygular ve CPU-GPU senkronizasyonunu sağlar.
 */
class VulkanCommandBufferManager {
public:
    VulkanCommandBufferManager();
    ~VulkanCommandBufferManager();

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const VulkanCommandBufferManagerConfig& config);
    void Shutdown();
    
    // Frame yönetimi
    bool BeginFrame();
    bool EndFrame();
    uint32_t GetCurrentFrameIndex() const { return m_currentFrame; }
    
    // Komut buffer erişimi
    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const;
    
    // Senkronizasyon nesneleri
    VkSemaphore GetImageAvailableSemaphore(uint32_t frameIndex) const;
    VkSemaphore GetRenderFinishedSemaphore(uint32_t frameIndex) const;
    VkFence GetInFlightFence(uint32_t frameIndex) const;
    
    // Komut kaydı
    bool ResetCommandBuffer(uint32_t frameIndex);
    bool BeginCommandBuffer(uint32_t frameIndex);
    bool EndCommandBuffer(uint32_t frameIndex);
    
    // Durum sorgulama
    bool IsInitialized() const { return m_isInitialized; }
    uint32_t GetMaxFramesInFlight() const { return m_maxFramesInFlight; }
    
    // Hata yönetimi
    std::string GetLastError() const { return m_lastError; }

private:
    // Bileşen başlatma yardımcıları
    bool CreateCommandPool();
    bool AllocateCommandBuffers();
    bool CreateSynchronizationObjects();
    
    // Temizleme yardımcıları
    void DestroyCommandPool();
    void DestroyCommandBuffers();
    void DestroySynchronizationObjects();
    
    // Hata yönetimi
    void SetLastError(const std::string& error);
    void HandleVulkanError(VkResult result, const std::string& operation);
    
    // Member variables
    VulkanCommandBufferManagerConfig m_config;
    uint32_t m_maxFramesInFlight;
    
    // Vulkan device reference
    VulkanDevice* m_device;
    
    // Command management
    std::unique_ptr<VulkanCommandPool> m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    // Synchronization objects
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    
    // Frame management
    uint32_t m_currentFrame;
    
    // State management
    bool m_isInitialized;
    std::string m_lastError;
};

} // namespace AstralEngine
