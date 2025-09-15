#pragma once

#include "Core/Logger.h"
#include "Subsystems/Platform/Window.h"
#include "VulkanInstance.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AstralEngine {

/**
 * @class VulkanDevice
 * @brief Vulkan cihaz yönetimi için temel sınıf
 * 
 * Bu sınıf, fiziksel GPU seçimi, mantıksal cihaz oluşturma,
 * kuyruk yönetimi ve pencere yüzeyi oluşturma gibi tüm
 * cihazla ilgili Vulkan işlemlerini yönetir.
 */
class VulkanDevice {
public:
    /**
     * @brief Kuyruk aileleri için indeks bilgilerini tutan yapı
     */
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;  ///< Grafik komutları için kuyruk ailesi
        std::optional<uint32_t> presentFamily;   ///< Sunum (presentation) için kuyruk ailesi

        /**
         * @brief Tüm gerekli kuyruk ailelerinin bulunup bulunmadığını kontrol eder
         * @return true if all required queue families are found
         */
        bool IsComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    /**
     * @brief Cihaz yapılandırması için ayarlar
     */
    struct Config {
        bool enableValidationLayers = true;      ///< Validation layer'ları etkinleştir
        std::vector<const char*> requiredExtensions; ///< Gerekli cihaz extension'ları
    };

    VulkanDevice();
    ~VulkanDevice();

    // Lifecycle management
    bool Initialize(VulkanInstance* instance, Window* window);
    void Shutdown(VkInstance instance);

    // Getters
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }
    const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_queueFamilyIndices; }
    
    // Device properties and features
    VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const { return m_deviceProperties; }
    VkPhysicalDeviceFeatures GetPhysicalDeviceFeatures() const { return m_deviceFeatures; }
    VkPhysicalDeviceMemoryProperties GetMemoryProperties() const { return m_memoryProperties; }
    
    // Error handling
    const std::string& GetLastError() const { return m_lastError; }
    
    // Configuration
    const Config& GetConfig() const { return m_config; }
    void UpdateConfig(const Config& config);
    
    // Memory management
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    // Device selection and creation
    bool PickPhysicalDevice(VkInstance instance);
    bool IsDeviceSuitable(VkPhysicalDevice device);
    int RateDeviceSuitability(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    
    // Logical device creation
    bool CreateLogicalDevice();
    bool CreateDeviceQueues();
    
    // Surface management
    bool CreateSurface(VulkanInstance* instance, Window* window);
    
    // Query device properties
    void QueryDeviceProperties();
    void QueryDeviceFeatures();
    void QueryMemoryProperties();
    
    // Helper functions
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
    
    // Error handling
    void SetError(const std::string& error);
    void HandleVulkanError(VkResult result, const std::string& operation);
    
    // Member variables
    Config m_config;
    std::string m_lastError;
    
    // Vulkan handles
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    
    // Queues
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    
    // Queue family indices
    QueueFamilyIndices m_queueFamilyIndices;
    
    // Device properties and features
    VkPhysicalDeviceProperties m_deviceProperties{};
    VkPhysicalDeviceFeatures m_deviceFeatures{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    
    // State management
    bool m_isInitialized = false;
    
    // Required device extensions
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

} // namespace AstralEngine
