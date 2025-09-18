#include "VulkanDevice.h"
#include "VulkanInstance.h"
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
    , m_transferQueue(VK_NULL_HANDLE)
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
    
    if (!instance) {
        SetError("Invalid instance pointer");
        return false;
    }
    
    try {
        Logger::Info("VulkanDevice", "Initializing VulkanDevice...");
        
        // Step 1: Create surface first (needed for queue family selection)
        if (m_config.surface != VK_NULL_HANDLE) {
            m_surface = m_config.surface;
            Logger::Info("VulkanDevice", "Using provided surface");
        } else if (window) {
            if (!CreateSurface(instance, window)) {
                Logger::Error("VulkanDevice", "Failed to create surface: " + GetLastError());
                return false;
            }
        } else {
            SetError("No surface provided and no window to create surface");
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
        m_transferQueue = VK_NULL_HANDLE;
        
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
        
        // Check for transfer queue family (dedicated transfer queue is better)
        if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && 
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.transferFamily = i;
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
    
    // If no dedicated transfer queue found, use graphics queue
    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.transferFamily = indices.graphicsFamily;
    }
    
    return indices;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    auto extensionsToCheck = m_config.deviceExtensions.empty() ? m_deviceExtensions : m_config.deviceExtensions;
    std::set<std::string> requiredExtensions(extensionsToCheck.begin(), extensionsToCheck.end());
    
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
    auto extensionsToUse = m_config.deviceExtensions.empty() ? m_deviceExtensions : m_config.deviceExtensions;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionsToUse.size());
    createInfo.ppEnabledExtensionNames = extensionsToUse.data();
    
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
    
    // Get transfer queue if available
    if (m_queueFamilyIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.transferFamily.value(), 0, &m_transferQueue);
    } else {
        // If no dedicated transfer queue, use graphics queue
        m_transferQueue = m_graphicsQueue;
    }
    
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

bool VulkanDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        SetError("Failed to create buffer");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        SetError("Failed to allocate buffer memory");
        return false;
    }

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
    return true;
}

void VulkanDevice::SubmitSingleTimeCommands(std::function<void(VkCommandBuffer)> commandFunction) {
    // Bu fonksiyon, GraphicsDevice'daki ana command pool'u kullanmalı.
    // Şimdilik geçici bir command pool oluşturuyoruz. Daha sonra bunu refactor edebiliriz.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool commandPool;
    vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    commandFunction(commandBuffer);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(m_device, commandPool, nullptr);
}

VulkanDevice::SwapChainSupportDetails VulkanDevice::QuerySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    
    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    
    // Get surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }
    
    // Get present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }
    
    return details;
}

void VulkanDevice::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    SubmitSingleTimeCommands([&](VkCommandBuffer commandBuffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });
}

VkImageView VulkanDevice::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        SetError("Failed to create texture image view");
        throw std::runtime_error(m_lastError);
    }
    return imageView;
}

void VulkanDevice::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        SetError("Failed to create image");
        throw std::runtime_error(m_lastError);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        SetError("Failed to allocate image memory");
        throw std::runtime_error(m_lastError);
    }

    vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkFence VulkanDevice::SubmitToTransferQueue(std::function<void(VkCommandBuffer)> commandFunction) {
    if (!m_device || !m_transferQueue) {
        SetError("Device or transfer queue not initialized");
        return VK_NULL_HANDLE;
    }

    // Create fence for synchronization
    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0; // Not signaled
    
    if (vkCreateFence(m_device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        SetError("Failed to create fence for transfer operation");
        return VK_NULL_HANDLE;
    }

    // Create temporary command pool for transfer operations
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndices.transferFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        SetError("Failed to create transfer command pool");
        vkDestroyFence(m_device, fence, nullptr);
        return VK_NULL_HANDLE;
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        SetError("Failed to allocate transfer command buffer");
        vkDestroyCommandPool(m_device, commandPool, nullptr);
        vkDestroyFence(m_device, fence, nullptr);
        return VK_NULL_HANDLE;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        SetError("Failed to begin transfer command buffer");
        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, nullptr);
        vkDestroyFence(m_device, fence, nullptr);
        return VK_NULL_HANDLE;
    }

    // Record user commands
    commandFunction(commandBuffer);

    // End command buffer
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        SetError("Failed to end transfer command buffer");
        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, nullptr);
        vkDestroyFence(m_device, fence, nullptr);
        return VK_NULL_HANDLE;
    }

    // Submit to transfer queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(m_transferQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        SetError("Failed to submit transfer commands");
        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, nullptr);
        vkDestroyFence(m_device, fence, nullptr);
        return VK_NULL_HANDLE;
    }

    // Clean up command buffer and pool (fence is returned to caller)
    vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(m_device, commandPool, nullptr);

    Logger::Debug("VulkanDevice", "Transfer commands submitted successfully with fence");
    return fence;
}

} // namespace AstralEngine
