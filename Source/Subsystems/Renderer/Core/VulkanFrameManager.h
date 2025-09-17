#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>

namespace AstralEngine {

class VulkanDevice;
class VulkanSwapchain;
class VulkanBuffer;

/**
 * @class VulkanFrameManager
 * @brief Frame bazlı Vulkan kaynaklarını yöneten sınıf
 * 
 * Bu sınıf, GraphicsDevice'dan ayrıştırılmış frame bazlı kaynakları yönetir:
 * - Command buffer'lar ve pool
 * - Descriptor set'ler, pool ve layout
 * - Uniform buffer'lar
 * - Frame senkronizasyon nesneleri (semaphore, fence)
 * 
 * Tek Sorumluluk Prensibi'ne göre tasarlanmıştır.
 */
class VulkanFrameManager {
public:
    VulkanFrameManager();
    ~VulkanFrameManager();

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout, uint32_t maxFramesInFlight);
    void Shutdown();

    // Frame yönetimi
    bool BeginFrame();
    bool EndFrame();
    bool WaitForFrame();

    // Resource erişimi
    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkDescriptorSet GetCurrentDescriptorSet(uint32_t frameIndex) const;
    VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }
    VkBuffer GetCurrentUniformBuffer(uint32_t frameIndex) const;
    VulkanBuffer* GetCurrentUniformBufferWrapper(uint32_t frameIndex) const;
    
    // Frame bilgileri
    uint32_t GetCurrentFrameIndex() const { return m_currentFrameIndex; }
    uint32_t GetCurrentImageIndex() const { return m_imageIndex; }
    uint32_t GetMaxFramesInFlight() const { return m_maxFramesInFlight; }

    // Swapchain recreation
    void RecreateSwapchain(VulkanSwapchain* newSwapchain);

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Resource oluşturma ve temizleme
    bool CreateCommandBuffers();
    bool CreateDescriptorPool();
    bool CreateDescriptorSets();
    bool CreateUniformBuffers();
    bool CreateSynchronizationObjects();
    
    void CleanupFrameResources();
    void CleanupSwapchainResources();

    // Yardımcı metodlar
    void SetError(const std::string& error);
    void ClearError();
    bool AcquireNextImage();
    bool PresentImage();

    // Member değişkenler
    VulkanDevice* m_device;
    VulkanSwapchain* m_swapchain;
    uint32_t m_maxFramesInFlight;
    std::string m_lastError;
    bool m_initialized;

    // Frame yönetimi
    uint32_t m_currentFrameIndex;
    uint32_t m_imageIndex;
    bool m_frameStarted;

    // Command buffer yönetimi
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Descriptor yönetimi
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    // Uniform buffer yönetimi
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;

    // Frame senkronizasyon nesneleri
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
};

} // namespace AstralEngine
