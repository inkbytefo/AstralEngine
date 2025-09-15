#include "VulkanDevice.h"
#include "Core/Logger.h"
#include <set>
#include <algorithm>

namespace AstralEngine {

VulkanDevice::VulkanDevice() 
    : m_physicalDevice(VK_NULL_HANDLE)
    , m_device(VK_NULL_HANDLE)
    , m_surface(VK_NULL_HANDLE)
    , m_graphicsQueue(VK_NULL_HANDLE)
    , m_presentQueue(VK_NULL_HANDLE)
    , m_isInitialized(false) {
    
    // Set default configuration
    m_config = Config{};
}

VulkanDevice::~VulkanDevice() {
    if (m_isInitialized) {
        // Note: We can't call Shutdown here without the instance
        // This should be handled by the owner (VulkanGraphicsContext)
        // For now, just log a warning
        Logger::Warning("VulkanDevice", "VulkanDevice destroyed without proper shutdown");
    }
}

bool VulkanDevice::Initialize(VulkanInstance* instance, Window* window) {
    if (m_isInitialized) {
        Logger::Error("VulkanDevice", "VulkanDevice already initialized");
        return false;
    }
    
    if (!instance || !window) {
        SetError("Invalid instance or window pointer");
        return false;
    }
    
    try {
        Logger::Info("VulkanDevice", "Initializing VulkanDevice...");
        
        // Step 1: Create surface first (needed for queue family selection)
        if (!CreateSurface(instance, window)) {
            Logger::Error("VulkanDevice", "Failed to create surface: " + GetLastError());
            return false;
        }
        
        // Step 2: Pick physical device
        if (!PickPhysicalDevice(instance->GetInstance())) {
            Logger::Error("VulkanDevice", "Failed to pick physical device: " + GetLastError());
            return false;
        }
        
        // Step 3: Create logical device
        if (!CreateLogicalDevice()) {
            Logger::Error("VulkanDevice", "Failed to create logical device: " + GetLastError());
            return false;
        }
        
        // Step 4: Get device queues
        if (!CreateDeviceQueues()) {
            Logger::Error("VulkanDevice", "Failed to create device queues: " + GetLastError());
            return false;
        }
        
        // Step 5: Query device properties and features
        QueryDeviceProperties();
        QueryDeviceFeatures();
        QueryMemoryProperties();
        
        m_isInitialized = true;
        Logger::Info("VulkanDevice", "VulkanDevice initialized successfully");
        Logger::Info("VulkanDevice", "Selected GPU: " + std::string(m_deviceProperties.deviceName));
        
        return true;
    }
    catch (const std::exception& e) {
        SetError("Initialization failed: " + std::string(e.what()));
        Logger::Error("VulkanDevice", "VulkanDevice initialization failed: " + GetLastError());
        return false;
    }
}

void VulkanDevice::Shutdown(VkInstance instance) {
    if (!m_isInitialized) {
        return;
    }
    
    try {
        Logger::Info("VulkanDevice", "Shutting down VulkanDevice...");
        
        // Wait for device to finish all operations
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
        }
        
        // Destroy logical device (this also destroys queues)
        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
        
        // Destroy surface
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        
        // Reset handles
        m_physicalDevice = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
        m_presentQueue = VK_NULL_HANDLE;
        
        m_isInitialized = false;
        Logger::Info("VulkanDevice", "VulkanDevice shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanDevice", "VulkanDevice shutdown failed: " + std::string(e.what()));
    }
}

void VulkanDevice::UpdateConfig(const Config& config) {
    m_config = config;
    // TODO: Reinitialize if configuration changes significantly
}

bool VulkanDevice::PickPhysicalDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "enumerate physical devices");
        return false;
    }
    
    if (deviceCount == 0) {
        SetError("No Vulkan-capable GPUs found");
        return false;
    }
    
    Logger::Info("VulkanDevice", "Found " + std::to_string(deviceCount) + " Vulkan-capable GPUs");
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "get physical devices");
        return false;
    }
    
    // Use the scoring system to pick the best device
    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    
    for (const auto& device : devices) {
        int score = RateDeviceSuitability(device);
        
        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }
    
    if (bestDevice == VK_NULL_HANDLE) {
        SetError("No suitable GPU found");
        return false;
    }
    
    m_physicalDevice = bestDevice;
    m_queueFamilyIndices = FindQueueFamilies(m_physicalDevice);
    
    return true;
}

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device) {
    // Check queue families
    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete()) {
        return false;
    }
    
    // Check device extension support
    if (!CheckDeviceExtensionSupport(device)) {
        return false;
    }
    
    // Check swapchain support (basic check)
    // More detailed swapchain checks will be done in VulkanSwapchain
    return true;
}

int VulkanDevice::RateDeviceSuitability(VkPhysicalDevice device) {
    int score = 0;
    std::string reasons;
    
    // Get device properties
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    
    // Get device features
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    
    // Check if device is suitable at all
    if (!IsDeviceSuitable(device)) {
        return -1;
    }
    
    // Discrete GPUs have a significant performance advantage
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
        reasons += "Discrete GPU (+1000)\n";
    } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
        reasons += "Integrated GPU (+500)\n";
    } else {
        score += 100;
        reasons += "Other GPU type (+100)\n";
    }
    
    // Maximum possible size of textures affects graphics quality
    score += static_cast<int>(properties.limits.maxImageDimension2D);
    reasons += "Max texture size: " + std::to_string(properties.limits.maxImageDimension2D) + "\n";
    
    // Check for required features
    if (features.geometryShader) {
        score += 100;
        reasons += "Geometry shader supported (+100)\n";
    }
    
    if (features.tessellationShader) {
        score += 100;
        reasons += "Tessellation shader supported (+100)\n";
    }
    
    Logger::Info("VulkanDevice", "Device '" + std::string(properties.deviceName) + "' score: " + std::to_string(score));
    Logger::Info("VulkanDevice", "Reasons:\n" + reasons);
    
    return score;
}

VulkanDevice::QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // Check for graphics queue family
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        
        // Check for presentation support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        
        // Early exit if we found both
        if (indices.IsComplete()) {
            break;
        }
        
        i++;
    }
    
    return indices;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
    
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    
    return requiredExtensions.empty();
}

bool VulkanDevice::CreateLogicalDevice() {
    // Get queue family indices
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);
    if (!indices.IsComplete()) {
        SetError("Required queue families not found");
        return false;
    }
    
    // Create queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    // Device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    // Device create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    // Device extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
    
    // Validation layers (deprecated but included for compatibility)
    if (m_config.enableValidationLayers) {
        // Note: Device validation layers are deprecated in Vulkan 1.3+
        // Instance validation layers are sufficient for most cases
    }
    
    // Create logical device
    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "create logical device");
        return false;
    }
    
    Logger::Info("VulkanDevice", "Logical device created successfully");
    return true;
}

bool VulkanDevice::CreateDeviceQueues() {
    // Get graphics queue
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    
    // Get present queue
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue);
    
    Logger::Info("VulkanDevice", "Device queues retrieved successfully");
    return true;
}

bool VulkanDevice::CreateSurface(VulkanInstance* instance, Window* window) {
    Logger::Info("VulkanDevice", "Creating Vulkan surface...");
    Logger::Info("VulkanDevice", "Instance: {}", reinterpret_cast<uintptr_t>(instance->GetInstance()));
    Logger::Info("VulkanDevice", "Window: {}", reinterpret_cast<uintptr_t>(window));
    
    // Use Window's CreateVulkanSurface method
    if (!window->CreateVulkanSurface(instance->GetInstance(), &m_surface)) {
        SetError("Failed to create Vulkan surface from window");
        Logger::Error("VulkanDevice", "CreateVulkanSurface failed - this is likely the root cause");
        return false;
    }
    
    Logger::Info("VulkanDevice", "Vulkan surface created successfully: {}", reinterpret_cast<uintptr_t>(m_surface));
    return true;
}

void VulkanDevice::QueryDeviceProperties() {
    if (m_physicalDevice != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    }
}

void VulkanDevice::QueryDeviceFeatures() {
    if (m_physicalDevice != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    }
}

void VulkanDevice::QueryMemoryProperties() {
    if (m_physicalDevice != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);
    }
}

bool VulkanDevice::CheckValidationLayerSupport() {
    // This is now handled at instance level
    // Device validation layers are deprecated in Vulkan 1.3+
    return true;
}

std::vector<const char*> VulkanDevice::GetRequiredExtensions() {
    std::vector<const char*> extensions;
    
    // Add required extensions from config
    for (const auto& ext : m_config.requiredExtensions) {
        extensions.push_back(ext);
    }
    
    return extensions;
}

void VulkanDevice::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanDevice", error);
}

void VulkanDevice::HandleVulkanError(VkResult result, const std::string& operation) {
    std::string errorString;
    
    switch (result) {
        case VK_SUCCESS:
            return; // No error
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            errorString = "Out of host memory";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            errorString = "Out of device memory";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            errorString = "Initialization failed";
            break;
        case VK_ERROR_DEVICE_LOST:
            errorString = "Device lost";
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            errorString = "Surface lost";
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            errorString = "Extension not present";
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            errorString = "Feature not present";
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            errorString = "Too many objects";
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            errorString = "Format not supported";
            break;
        case VK_ERROR_FRAGMENTED_POOL:
            errorString = "Fragmented pool";
            break;
        case VK_ERROR_UNKNOWN:
            errorString = "Unknown error";
            break;
        default:
            errorString = "Unknown Vulkan error (" + std::to_string(result) + ")";
            break;
    }
    
    SetError("Vulkan error during " + operation + ": " + errorString);
}

uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    Logger::Debug("VulkanDevice", "Finding memory type: typeFilter={}, properties={}", 
                 typeFilter, static_cast<uint32_t>(properties));
    
    // Fiziksel cihazın bellek özelliklerini al
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    // Uygun bellek tipini ara
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Bellek tipi filter'da var mı ve istenen özelliklere sahip mi?
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            Logger::Debug("VulkanDevice", "Found suitable memory type: {}", i);
            return i;
        }
    }
    
    Logger::Error("VulkanDevice", "Failed to find suitable memory type");
    SetError("Failed to find suitable memory type");
    return UINT32_MAX;
}

} // namespace AstralEngine
