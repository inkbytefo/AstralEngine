#include "VulkanInstance.h"
#include "../../../Core/Logger.h"
#include <iostream>
#include <set>
#include <cstring>
#include <algorithm>

// Vulkan debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)pUserData; // Suppress unused parameter warning
    
    std::string severity;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity = "VERBOSE";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity = "INFO";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity = "WARNING";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity = "ERROR";
    }
    
    std::string type;
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type = "PERFORMANCE";
    }
    
    // Use engine logger instead of std::cout
    std::string message = "[Vulkan Debug][" + severity + "][" + type + "]: " + pCallbackData->pMessage;
    
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        AstralEngine::Logger::Error("VulkanInstance", message);
        return VK_TRUE; // Abort on error
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        AstralEngine::Logger::Warning("VulkanInstance", message);
    } else {
        AstralEngine::Logger::Debug("VulkanInstance", message);
    }
    
    return VK_FALSE;
}

namespace AstralEngine {

VulkanInstance::VulkanInstance() 
    : m_instance(nullptr)
    , m_debugMessenger(nullptr)
    , m_initialized(false) {
    
    // Set default configuration
    m_config = Config{};
    m_config.apiVersion = VK_API_VERSION_1_4;
    m_config.validationLayers = {"VK_LAYER_KHRONOS_validation"};
    m_config.instanceExtensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
#ifdef _WIN32
        ,VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#else
        ,VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
    };
}

VulkanInstance::~VulkanInstance() {
    if (m_initialized) {
        Shutdown();
    }
}

bool VulkanInstance::Initialize(const Config& config) {
    if (m_initialized) {
        m_lastError = "Instance already initialized";
        return false;
    }
    
    m_config = config;
    
    try {
        // Validate configuration
        if (!ValidateConfiguration()) {
            return false;
        }
        
        // Query available extensions and layers
        if (!QueryExtensionsAndLayers()) {
            return false;
        }
        
        // Create Vulkan instance
        if (!CreateInstance()) {
            return false;
        }
        
        // Debug callback is set up during instance creation, no need for a separate call here.
        
        // Query physical devices
        if (!QueryPhysicalDevices()) {
            return false;
        }
        
        m_initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = "Initialization failed: " + std::string(e.what());
        return false;
    }
}

void VulkanInstance::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    try {
        // Destroy debug callback
        if (m_debugMessenger) {
            DestroyDebugCallback();
        }
        
        // Destroy instance
        if (m_instance) {
            DestroyInstance();
        }
        
        // Clear physical devices
        m_physicalDevices.clear();
        m_supportedExtensions.clear();
        m_supportedLayers.clear();
        
        m_initialized = false;
        m_lastError.clear();
    }
    catch (const std::exception& e) {
        m_lastError = "Shutdown failed: " + std::string(e.what());
    }
}

bool VulkanInstance::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_config.applicationName.c_str();
    appInfo.applicationVersion = m_config.applicationVersion;
    appInfo.pEngineName = m_config.engineName.c_str();
    appInfo.engineVersion = m_config.engineVersion;
    appInfo.apiVersion = m_config.apiVersion;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // Add extensions
    std::vector<const char*> enabledExtensions;
    for (const auto& ext : m_config.instanceExtensions) {
        bool isSupported = IsExtensionSupported(ext);
        if (isSupported) {
            enabledExtensions.push_back(ext.c_str());
            Logger::Debug("VulkanInstance", "Extension enabled: {}", ext);
        } else {
            if (ext == VK_EXT_DEBUG_UTILS_EXTENSION_NAME && m_config.enableDebugUtils) {
                m_lastError = "Debug extension not supported: " + ext;
                Logger::Error("VulkanInstance", m_lastError);
                return false;
            } else if (ext != VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
                m_lastError = "Extension not supported: " + ext;
                Logger::Error("VulkanInstance", m_lastError);
                return false;
            }
        }
    }
    
    Logger::Info("VulkanInstance", "Total enabled extensions: {}", enabledExtensions.size());
    for (const auto& ext : enabledExtensions) {
        Logger::Debug("VulkanInstance", "Enabled extension: {}", ext);
    }
    
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    
    // Add validation layers
    std::vector<const char*> enabledLayers;
    if (m_config.enableValidationLayers) {
        for (const auto& layer : m_config.validationLayers) {
            if (IsLayerSupported(layer)) {
                enabledLayers.push_back(layer.c_str());
            } else {
                m_lastError = "Validation layer not supported: " + layer;
                return false;
            }
        }
    }
    
    createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
    createInfo.ppEnabledLayerNames = enabledLayers.data();
    
    // Enable debug messenger during instance creation
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_config.enableDebugUtils) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;
        debugCreateInfo.pUserData = nullptr;
        
        createInfo.pNext = &debugCreateInfo;
    }
    
    Logger::Info("VulkanInstance", "Creating Vulkan instance...");
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to create Vulkan instance: " + GetVulkanResultString(result);
        Logger::Error("VulkanInstance", m_lastError);
        return false;
    }
    Logger::Info("VulkanInstance", "Vulkan instance created successfully!");
    
    return true;
}



bool VulkanInstance::QueryPhysicalDevices() {
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        m_lastError = "Failed to query physical device count: " + GetVulkanResultString(result);
        return false;
    }
    
    m_physicalDevices.resize(deviceCount);
    result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, m_physicalDevices.data());
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to enumerate physical devices: " + GetVulkanResultString(result);
        return false;
    }
    
    return true;
}

bool VulkanInstance::QueryExtensionsAndLayers() {
    // Query extensions
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to query extension count: " + GetVulkanResultString(result);
        return false;
    }
    
    std::vector<VkExtensionProperties> extensions(extensionCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to enumerate extensions: " + GetVulkanResultString(result);
        return false;
    }
    
    m_supportedExtensions.clear();
    for (const auto& ext : extensions) {
        m_supportedExtensions.push_back(ext.extensionName);
        std::cout << "[VulkanInstance] Available extension: " << ext.extensionName << std::endl;
    }
    
    // Query layers
    uint32_t layerCount = 0;
    result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to query layer count: " + GetVulkanResultString(result);
        return false;
    }
    
    std::vector<VkLayerProperties> layers(layerCount);
    result = vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    if (result != VK_SUCCESS) {
        m_lastError = "Failed to enumerate layers: " + GetVulkanResultString(result);
        return false;
    }
    
    m_supportedLayers.clear();
    for (const auto& layer : layers) {
        m_supportedLayers.push_back(layer.layerName);
    }
    
    return true;
}

bool VulkanInstance::ValidateConfiguration() {
    if (m_config.applicationName.empty()) {
        m_lastError = "Application name cannot be empty";
        return false;
    }
    
    if (m_config.engineName.empty()) {
        m_lastError = "Engine name cannot be empty";
        return false;
    }
    
    if (m_config.apiVersion == 0) {
        m_lastError = "API version must be specified";
        return false;
    }
    
    return true;
}

void VulkanInstance::DestroyInstance() {
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = nullptr;
    }
}

void VulkanInstance::DestroyDebugCallback() {
    if (!m_debugMessenger || !m_instance) {
        return;
    }
    
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func) {
        func(m_instance, m_debugMessenger, nullptr);
    }
    
    m_debugMessenger = nullptr;
}

uint32_t VulkanInstance::GetPhysicalDeviceCount() const {
    return static_cast<uint32_t>(m_physicalDevices.size());
}

VkPhysicalDevice VulkanInstance::GetPhysicalDevice(uint32_t index) const {
    if (index >= m_physicalDevices.size()) {
        return nullptr;
    }
    return m_physicalDevices[index];
}

std::vector<VkPhysicalDevice> VulkanInstance::GetPhysicalDevices() const {
    return m_physicalDevices;
}

bool VulkanInstance::IsExtensionSupported(const std::string& extensionName) const {
    return std::find(m_supportedExtensions.begin(), m_supportedExtensions.end(), extensionName) != m_supportedExtensions.end();
}

bool VulkanInstance::IsLayerSupported(const std::string& layerName) const {
    return std::find(m_supportedLayers.begin(), m_supportedLayers.end(), layerName) != m_supportedLayers.end();
}

std::vector<std::string> VulkanInstance::GetSupportedExtensions() const {
    return m_supportedExtensions;
}

std::vector<std::string> VulkanInstance::GetSupportedLayers() const {
    return m_supportedLayers;
}

std::vector<std::string> VulkanInstance::GetEnabledExtensions() const {
    return m_config.instanceExtensions;
}

std::vector<std::string> VulkanInstance::GetEnabledLayers() const {
    return m_config.validationLayers;
}

VkResult VulkanInstance::CreateSurface(void* windowHandle, VkSurfaceKHR* surface) const {
    if (!m_instance || !windowHandle || !surface) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = static_cast<HWND>(windowHandle);
    createInfo.hinstance = GetModuleHandle(nullptr);
    
    return vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, surface);
#else
    // Linux implementation would go here
    return VK_ERROR_INITIALIZATION_FAILED;
#endif
}

void VulkanInstance::DestroySurface(VkSurfaceKHR surface) const {
    if (m_instance && surface) {
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

std::string VulkanInstance::GetVulkanVersionString() const {
    uint32_t version = m_config.apiVersion;
    
    std::string result = std::to_string(VK_VERSION_MAJOR(version)) + "." +
                        std::to_string(VK_VERSION_MINOR(version)) + "." +
                        std::to_string(VK_VERSION_PATCH(version));
    
    return result;
}

std::string VulkanInstance::GetVulkanResultString(VkResult result) const {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT: return "VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT";
        default: return "Unknown VkResult";
    }
}

} // namespace AstralEngine
