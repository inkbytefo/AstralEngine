#pragma once

#include "../../../Core/ISubsystem.h"
#include "../../../Core/Engine.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <string>

// Forward declarations
namespace AstralEngine {
class VulkanInstance;
class VulkanDevice;
class VulkanSwapchain;
}

namespace AstralEngine {

/**
 * @struct VulkanGraphicsContextConfig
 * @brief Vulkan graphics context yapılandırma parametreleri
 */
struct VulkanGraphicsContextConfig {
    std::string applicationName = "Astral Engine";
    uint32_t applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    std::string engineName = "Astral Engine";
    uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
    
    bool enableValidationLayers = true;
    uint32_t windowWidth = 1920;
    uint32_t windowHeight = 1080;
};

/**
 * @class VulkanGraphicsContext
 * @brief Vulkan core bileşenlerini yöneten context sınıfı
 * 
 * Bu sınıf, Vulkan instance, device ve swapchain gibi temel
 * bileşenlerin yaşam döngüsünü yönetir. Renderer'ın alt katmanı olarak
 * hizmet verir ve Vulkan'a özgü işlemleri kapsüller.
 */
class VulkanGraphicsContext {
public:
    VulkanGraphicsContext();
    ~VulkanGraphicsContext();

    // Yaşam döngüsü
    bool Initialize(Engine* owner, const VulkanGraphicsContextConfig& config);
    void Shutdown();
    
    // Getter metodlar
    VulkanInstance* GetInstance() const { return m_instance.get(); }
    VulkanDevice* GetDevice() const { return m_device.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_swapchain.get(); }
    
    // Vulkan nesnelerine doğrudan erişim
    VkInstance GetVkInstance() const;
    VkDevice GetVkDevice() const;
    VkPhysicalDevice GetVkPhysicalDevice() const;
    VkQueue GetGraphicsQueue() const;
    VkQueue GetPresentQueue() const;
    
    // Yapılandırma
    const VulkanGraphicsContextConfig& GetConfig() const { return m_config; }
    void UpdateConfig(const VulkanGraphicsContextConfig& config);
    
    // Durum sorgulama
    bool IsInitialized() const { return m_isInitialized; }
    
    // Swapchain yönetimi
    bool RecreateSwapchain();
    VkExtent2D GetSwapchainExtent() const;
    uint32_t GetSwapchainImageCount() const;
    
    // Hata yönetimi
    std::string GetLastError() const { return m_lastError; }

private:
    // Bileşen başlatma yardımcıları
    bool InitializeInstance();
    bool InitializeDevice();
    bool InitializeSwapchain();
    
    // Hata yönetimi
    void SetLastError(const std::string& error);
    
    // Member variables
    VulkanGraphicsContextConfig m_config;
    
    // Core Vulkan components
    std::unique_ptr<VulkanInstance> m_instance;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    
    // Engine reference
    Engine* m_owner;
    
    // State management
    bool m_isInitialized;
    std::string m_lastError;
};

} // namespace AstralEngine
