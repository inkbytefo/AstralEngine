#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace AstralEngine {

/**
 * @struct Config
 * @brief Vulkan instance yapılandırma parametreleri
 */
struct Config {
    std::string applicationName = "Astral Engine";
    uint32_t applicationVersion = 1;
    std::string engineName = "Astral Engine";
    uint32_t engineVersion = 1;
    uint32_t apiVersion = 0; // VK_API_VERSION_1_4
    
    bool enableValidationLayers = true;
    bool enableDebugUtils = true;
    std::vector<std::string> validationLayers;
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> deviceExtensions;
    
    // Debug settings
    bool enableDebugCallback = true;
    bool enableVerboseLogging = false;
    std::string logFilePath = "vulkan_instance.log";
};

/**
 * @class VulkanInstance
 * @brief Vulkan instance yönetimi
 * 
 * Vulkan instance oluşturma, validation layers, extensions ve debug utils yönetimi.
 */
class VulkanInstance {
public:
    VulkanInstance();
    ~VulkanInstance();

    bool Initialize(const Config& config = Config());
    void Shutdown();

    // Instance management
    VkInstance GetInstance() const { return m_instance; }
    const Config& GetConfig() const { return m_config; }
    
    // Physical device management
    uint32_t GetPhysicalDeviceCount() const;
    VkPhysicalDevice GetPhysicalDevice(uint32_t index) const;
    std::vector<VkPhysicalDevice> GetPhysicalDevices() const;
    
    // Extension support
    bool IsExtensionSupported(const std::string& extensionName) const;
    bool IsLayerSupported(const std::string& layerName) const;
    std::vector<std::string> GetSupportedExtensions() const;
    std::vector<std::string> GetSupportedLayers() const;
    std::vector<std::string> GetEnabledExtensions() const;
    std::vector<std::string> GetEnabledLayers() const;
    
    // Query methods
    bool QueryExtensionsAndLayers();
    
    // Debug utils
    bool IsDebugUtilsEnabled() const { return m_config.enableDebugUtils; }
    VkDebugUtilsMessengerEXT GetDebugMessenger() const { return m_debugMessenger; }
    
    // Surface creation
    VkResult CreateSurface(void* windowHandle, VkSurfaceKHR* surface) const;
    void DestroySurface(VkSurfaceKHR surface) const;
    
    // Validation
    bool AreValidationLayersEnabled() const { return m_config.enableValidationLayers; }
    void SetValidationEnabled(bool enabled) { m_config.enableValidationLayers = enabled; }
    
    // Version info
    uint32_t GetVulkanAPIVersion() const { return m_config.apiVersion; }
    std::string GetVulkanVersionString() const;
    
    // Error handling
    std::string GetLastError() const { return m_lastError; }
    void ClearLastError() { m_lastError.clear(); }

private:
    bool CreateInstance();
    bool QueryPhysicalDevices();
    bool ValidateConfiguration();
    
    void DestroyInstance();
    void DestroyDebugCallback();
    
    std::string GetVulkanResultString(VkResult result) const;
    
    // Member variables
    Config m_config;
    VkInstance m_instance = nullptr;
    VkDebugUtilsMessengerEXT m_debugMessenger = nullptr;
    
    std::vector<VkPhysicalDevice> m_physicalDevices;
    std::vector<std::string> m_supportedExtensions;
    std::vector<std::string> m_supportedLayers;
    
    std::string m_lastError;
    bool m_initialized = false;
};

} // namespace AstralEngine
