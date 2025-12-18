#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <string>

#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
    #include "Core/VulkanInstance.h"
    #include "Core/VulkanDevice.h"
    #include "Core/VulkanSwapchain.h"
    #include "Core/VulkanFramebuffer.h"
    #include "VulkanMemoryManager.h"
    #include "VulkanSynchronization.h"
    #include "VulkanRenderer.h"
    #include "VulkanBindlessSystem.h"
    #include "Core/VulkanFrameManager.h"
    #include "Core/VulkanTransferManager.h"
#endif

// Forward declaration to break circular dependency
class VulkanBuffer;

namespace AstralEngine {

class Window;
class Engine;

/**
 * @struct GraphicsDeviceConfig
 * @brief GraphicsDevice yapılandırma ayarları
 */
struct GraphicsDeviceConfig {
    bool enableValidationLayers = true;           ///< Validation layer'ları etkinleştir
    bool enableDebugNames = true;                 ///< Debug isimlerini etkinleştir
    bool enableTimelineSemaphores = true;        ///< Timeline semaphore'ları etkinleştir
    uint32_t maxFramesInFlight = 2;              ///< Maksimum frame sayısı
    std::string applicationName = "AstralEngine"; ///< Uygulama adı
    std::string engineName = "AstralEngine";      ///< Motor adı
    uint32_t applicationVersion = VK_MAKE_VERSION(1, 0, 0); ///< Uygulama versiyonu
    uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);      ///< Motor versiyonu
    uint32_t apiVersion = VK_API_VERSION_1_4;     ///< Vulkan API versiyonu
};

/**
 * @class GraphicsDevice
 * @brief Modern Vulkan GraphicsDevice sınıfı
 * 
 * Bu sınıf, tüm Vulkan altyapısını (Instance, Device, Swapchain, Bellek Yöneticisi,
 * Senkronizasyon Yöneticisi) orkestra eden ana "context" sınıfıdır.
 * Modern Vulkan 1.3+ özelliklerini kullanır ve RAII prensibine göre tasarlanmıştır.
 * 
 * Sorumlulukları:
 * - Vulkan instance ve device yönetimi
 * - Swapchain oluşturma ve yönetimi
 * - Bellek yönetimi (VMA prensipleri ile)
 * - Senkronizasyon yönetimi (VK_KHR_synchronization2)
 * - Renderer ve diğer bileşenlerin yaşam döngüsü
 * - Debug ve validation desteği
 */
class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();

    // Non-copyable
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Yaşam döngüsü
    bool Initialize(Window* window, Engine* owner = nullptr, const GraphicsDeviceConfig& config = GraphicsDeviceConfig{});
    void Shutdown();

    // Frame yönetimi
    bool BeginFrame();
    bool EndFrame();
    bool WaitForFrame();
    void RecreateSwapchain();

    // Resource yönetimi
#ifdef ASTRAL_USE_VULKAN
    VulkanMemoryManager* GetMemoryManager() { return m_memoryManager.get(); }
    const VulkanMemoryManager* GetMemoryManager() const { return m_memoryManager.get(); }
    
    VulkanSynchronization* GetSynchronization() { return m_synchronization.get(); }
    const VulkanSynchronization* GetSynchronization() const { return m_synchronization.get(); }
    
    VulkanTransferManager* GetTransferManager() { return m_transferManager.get(); }
    const VulkanTransferManager* GetTransferManager() const { return m_transferManager.get(); }
#endif

    // Getters
#ifdef ASTRAL_USE_VULKAN
    // VulkanInstance wrapper access
    VulkanInstance* GetVulkanInstance() { return m_vulkanInstance.get(); }
    const VulkanInstance* GetVulkanInstance() const { return m_vulkanInstance.get(); }
    
    // VulkanDevice wrapper access
    VulkanDevice* GetVulkanDevice() { return m_vulkanDevice.get(); }
    const VulkanDevice* GetVulkanDevice() const { return m_vulkanDevice.get(); }
    
    // VulkanSwapchain access
    VulkanSwapchain* GetSwapchain() { return m_swapchain.get(); }
    const VulkanSwapchain* GetSwapchain() const { return m_swapchain.get(); }
    
    // VulkanRenderer access
    VulkanRenderer* GetVulkanRenderer() { return m_vulkanRenderer.get(); }
    const VulkanRenderer* GetVulkanRenderer() const { return m_vulkanRenderer.get(); }
    
    // Legacy getters for compatibility - delegate to wrappers
    VkInstance GetInstance() const { return m_vulkanInstance ? m_vulkanInstance->GetInstance() : VK_NULL_HANDLE; }
    VkDevice GetDevice() const { return m_vulkanDevice ? m_vulkanDevice->GetDevice() : VK_NULL_HANDLE; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_vulkanDevice ? m_vulkanDevice->GetPhysicalDevice() : VK_NULL_HANDLE; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    VkQueue GetGraphicsQueue() const { return m_vulkanDevice ? m_vulkanDevice->GetGraphicsQueue() : VK_NULL_HANDLE; }
    VkQueue GetPresentQueue() const { return m_vulkanDevice ? m_vulkanDevice->GetPresentQueue() : VK_NULL_HANDLE; }
    VkQueue GetTransferQueue() const { return m_vulkanDevice ? m_vulkanDevice->GetTransferQueue() : VK_NULL_HANDLE; }
    uint32_t GetGraphicsQueueFamily() const { 
        return m_vulkanDevice && m_vulkanDevice->GetQueueFamilyIndices().graphicsFamily.has_value() ? 
               m_vulkanDevice->GetQueueFamilyIndices().graphicsFamily.value() : 0; 
    }
    uint32_t GetPresentQueueFamily() const { 
        return m_vulkanDevice && m_vulkanDevice->GetQueueFamilyIndices().presentFamily.has_value() ? 
               m_vulkanDevice->GetQueueFamilyIndices().presentFamily.value() : 0; 
    }
    uint32_t GetTransferQueueFamily() const { 
        return m_vulkanDevice && m_vulkanDevice->GetQueueFamilyIndices().transferFamily.has_value() ? 
               m_vulkanDevice->GetQueueFamilyIndices().transferFamily.value() : 0; 
    }
#endif

    // Device capabilities ve durum bilgileri
    bool IsInitialized() const { return m_initialized; }
    bool IsValidationLayersEnabled() const { return m_config.enableValidationLayers; }
    bool IsTimelineSemaphoreSupported() const { return m_timelineSemaphoreSupported; }
    uint32_t GetCurrentFrameIndex() const { return m_currentFrameIndex; }
    uint32_t GetMaxFramesInFlight() const { return m_config.maxFramesInFlight; }
    
    // Frame bilgileri için getter'lar (VulkanRenderer tarafından kullanılacak)
    uint32_t GetCurrentImageIndex() const { return m_frameManager ? m_frameManager->GetCurrentImageIndex() : 0; }
    VkCommandBuffer GetCurrentCommandBuffer() const { 
        return m_frameManager ? m_frameManager->GetCurrentCommandBuffer() : VK_NULL_HANDLE; 
    }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return VK_NULL_HANDLE; }
    VulkanBindlessSystem* GetBindlessSystem() const { return m_bindlessSystem.get(); }
    VkDescriptorPool GetDescriptorPool() const { return m_frameManager ? m_frameManager->GetDescriptorPool() : VK_NULL_HANDLE; }
    VkDescriptorSet GetCurrentDescriptorSet(uint32_t frameIndex) const { 
        return m_frameManager ? m_frameManager->GetCurrentDescriptorSet(frameIndex) : VK_NULL_HANDLE; 
    }
    VkBuffer GetCurrentUniformBuffer(uint32_t frameIndex) const {
        return m_frameManager ? m_frameManager->GetCurrentUniformBuffer(frameIndex) : VK_NULL_HANDLE;
    }
    VulkanBuffer* GetCurrentUniformBufferWrapper(uint32_t frameIndex) const {
        return m_frameManager ? m_frameManager->GetCurrentUniformBufferWrapper(frameIndex) : nullptr;
    }
    VkBuffer GetCurrentUniformBuffer() const {
        return m_frameManager ? m_frameManager->GetCurrentUniformBuffer(m_currentFrameIndex) : VK_NULL_HANDLE;
    }
    VulkanBuffer* GetCurrentUniformBufferWrapper() const {
        return m_frameManager ? m_frameManager->GetCurrentUniformBufferWrapper(m_currentFrameIndex) : nullptr;
    }
    
    // Frame manager erişimi
    VulkanFrameManager* GetFrameManager() { return m_frameManager.get(); }
    const VulkanFrameManager* GetFrameManager() const { return m_frameManager.get(); }
    
    // Konfigürasyon erişimi
    const GraphicsDeviceConfig& GetConfig() const { return m_config; }
    
    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    
    // Debug ve istatistikler
    std::string GetDebugReport() const;
    void DumpMemoryMap() const;
    void CheckForLeaks() const;

    // Geçici kaynakları temizlemek için
    void QueueBufferForDeletion(VkBuffer buffer, VkDeviceMemory memory);

private:
    // Vulkan başlatma adımları
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool CreateSurface(Window* window);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateMemoryManager();
    bool CreateSynchronization();
    bool CreateRenderer();
    
    // Yardımcı metodlar
    bool CheckValidationLayerSupport();
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    std::vector<const char*> GetRequiredInstanceExtensions() const;
    std::vector<const char*> GetRequiredDeviceExtensions() const;
    bool IsDeviceSuitable(VkPhysicalDevice device);
    int RateDeviceSuitability(VkPhysicalDevice device);
    
    // Debug ve loglama
    void LogInitialization() const;
    void LogDeviceCapabilities() const;
    void SetError(const std::string& error);
    void ClearError();
    
    // Frame senkronizasyonu
    bool AcquireNextImage();
    bool PresentImage();
    
    // Swapchain recreation
    void CleanupSwapchain();
    
    
#ifdef ASTRAL_USE_VULKAN
    // Core Vulkan objects
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    
    // Deletion queue for temporary resources
    std::vector<std::vector<std::pair<VkBuffer, VkDeviceMemory>>> m_deletionQueue;

    // Debug
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
#endif

    // Member değişkenler
    Window* m_window = nullptr;
    Engine* m_owner = nullptr;
    GraphicsDeviceConfig m_config;
    std::string m_lastError;
    bool m_initialized = false;
    bool m_timelineSemaphoreSupported = false;
    
    // Frame yönetimi
    uint32_t m_currentFrameIndex = 0;
    uint32_t m_imageIndex = 0;
    bool m_frameStarted = false;
    
#ifdef ASTRAL_USE_VULKAN
    // Vulkan bileşenleri
    std::unique_ptr<VulkanInstance> m_vulkanInstance;
    std::unique_ptr<VulkanDevice> m_vulkanDevice;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::unique_ptr<VulkanMemoryManager> m_memoryManager;
    std::unique_ptr<VulkanSynchronization> m_synchronization;
    std::unique_ptr<VulkanRenderer> m_vulkanRenderer;
    std::unique_ptr<VulkanFrameManager> m_frameManager;
    std::unique_ptr<VulkanTransferManager> m_transferManager;
    std::unique_ptr<VulkanBindlessSystem> m_bindlessSystem;
#endif
    
    // Validation layers
    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    // Device extensions (modern Vulkan için güncellendi)
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME
    };
};

} // namespace AstralEngine
