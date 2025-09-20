#include "GraphicsDevice.h"
#include "../Platform/Window.h"
#include "../../Core/Logger.h"
#include "../../Core/Engine.h"
#include "Buffers/VulkanBuffer.h"

#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
    #include "Core/VulkanDevice.h"
    #include "Core/VulkanInstance.h"
    #include "Core/VulkanTransferManager.h"
#endif

namespace AstralEngine {

GraphicsDevice::GraphicsDevice() {
    Logger::Debug("GraphicsDevice", "GraphicsDevice created");
}

GraphicsDevice::~GraphicsDevice() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("GraphicsDevice", "GraphicsDevice destroyed");
}

bool GraphicsDevice::Initialize(Window* window, Engine* owner, const GraphicsDeviceConfig& config) {
    if (m_initialized) {
        Logger::Warning("GraphicsDevice", "GraphicsDevice already initialized");
        return true;
    }
    
    if (!window) {
        Logger::Error("GraphicsDevice", "Cannot initialize without a valid window");
        return false;
    }
    
    m_window = window;
    m_owner = owner;
    m_config = config;
    
    Logger::Info("GraphicsDevice", "Initializing Modern Vulkan GraphicsDevice...");
    Logger::Info("GraphicsDevice", "Configuration: Validation={}, Timeline={}, MaxFrames={}", 
                config.enableValidationLayers, config.enableTimelineSemaphores, config.maxFramesInFlight);
    
#ifdef ASTRAL_USE_VULKAN
    // Vulkan başlatma adımlarını sırayla gerçekleştir
    Logger::Info("GraphicsDevice", "Creating Vulkan instance...");
    if (!CreateInstance()) {
        Logger::Error("GraphicsDevice", "Failed to create Vulkan instance");
        return false;
    }
    Logger::Info("GraphicsDevice", "Vulkan instance created successfully");
    
    // Debug messenger is already set up during instance creation in VulkanInstance
    // No need to call SetupDebugMessenger separately
    
    if (!CreateSurface(window)) {
        Logger::Error("GraphicsDevice", "Failed to create surface");
        return false;
    }
    
    if (!PickPhysicalDevice()) {
        Logger::Error("GraphicsDevice", "Failed to pick physical device");
        return false;
    }
    
    if (!CreateLogicalDevice()) {
        Logger::Error("GraphicsDevice", "Failed to create logical device");
        return false;
    }
    
    if (!CreateSwapchain()) {
        Logger::Error("GraphicsDevice", "Failed to create swapchain");
        return false;
    }
    
    if (!CreateMemoryManager()) {
        Logger::Error("GraphicsDevice", "Failed to create memory manager");
        return false;
    }
    
    if (!CreateSynchronization()) {
        Logger::Error("GraphicsDevice", "Failed to create synchronization manager");
        return false;
    }
    
    // Descriptor set layout'ı oluştur (frame'den bağımsız)
    Logger::Info("GraphicsDevice", "Creating descriptor set layout...");
    if (!CreateDescriptorSetLayout()) {
        Logger::Error("GraphicsDevice", "Failed to create descriptor set layout");
        return false;
    }
    
    // Frame manager'ı oluştur ve başlat
    Logger::Info("GraphicsDevice", "Creating frame manager...");
    m_frameManager = std::make_unique<VulkanFrameManager>();
    if (!m_frameManager->Initialize(m_vulkanDevice.get(), m_swapchain.get(), m_descriptorSetLayout, m_config.maxFramesInFlight)) {
        Logger::Error("GraphicsDevice", "Failed to initialize frame manager: {}", m_frameManager->GetLastError());
        return false;
    }
    
    Logger::Info("GraphicsDevice", "Frame manager created successfully");

    m_deletionQueue.resize(m_config.maxFramesInFlight);

    // Transfer manager'ı oluştur ve başlat
    Logger::Info("GraphicsDevice", "Creating transfer manager...");
    m_transferManager = std::make_unique<VulkanTransferManager>(m_vulkanDevice.get());
    m_transferManager->Initialize();
    Logger::Info("GraphicsDevice", "Transfer manager created successfully");

    if (!CreateRenderer()) {
        Logger::Error("GraphicsDevice", "Failed to create renderer");
        return false;
    }
    
    LogInitialization();
    LogDeviceCapabilities();
    
    Logger::Info("GraphicsDevice", "Modern Vulkan GraphicsDevice initialized successfully");
    
#else
    Logger::Error("GraphicsDevice", "Vulkan not available - cannot initialize GraphicsDevice");
    return false;
#endif
    
    m_initialized = true;
    return true;
}

void GraphicsDevice::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("GraphicsDevice", "Shutting down GraphicsDevice...");
    
#ifdef ASTRAL_USE_VULKAN
    // Cihazı idle bekle
    vkDeviceWaitIdle(m_vulkanDevice->GetDevice());

    // Deletion queue'daki her şeyi temizle
    for (auto& frame_queue : m_deletionQueue) {
        for (auto& pair : frame_queue) {
            vkDestroyBuffer(m_vulkanDevice->GetDevice(), pair.first, nullptr);
            vkFreeMemory(m_vulkanDevice->GetDevice(), pair.second, nullptr);
        }
        frame_queue.clear();
    }
    
    // Frame manager'ı kapat
    if (m_frameManager) {
        m_frameManager->Shutdown();
        m_frameManager.reset();
    }
    
    // Transfer manager'ı kapat
    if (m_transferManager) {
        m_transferManager->Shutdown();
        m_transferManager.reset();
    }
    
    // Renderer'ı kapat
    if (m_vulkanRenderer) {
        m_vulkanRenderer->Shutdown();
        m_vulkanRenderer.reset();
    }
    
    // Senkronizasyon yöneticisini kapat
    if (m_synchronization) {
        m_synchronization->Shutdown();
        m_synchronization.reset();
    }
    
    // Bellek yöneticisini kapat
    if (m_memoryManager) {
        m_memoryManager->Shutdown();
        m_memoryManager.reset();
    }
    
    // Swapchain'i temizle
    CleanupSwapchain();
    
    // VulkanDevice wrapper'ı kapat
    if (m_vulkanDevice) {
        m_vulkanDevice->Shutdown(m_vulkanInstance->GetInstance());
        m_vulkanDevice.reset();
    }
    
    // Surface'i temizle
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_vulkanInstance->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    
    // Debug messenger'ı temizle
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vulkanInstance->GetInstance(), "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_vulkanInstance->GetInstance(), m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }
    
    // VulkanInstance wrapper'ı kapat
    if (m_vulkanInstance) {
        m_vulkanInstance->Shutdown();
        m_vulkanInstance.reset();
    }
#endif
    
    m_initialized = false;
    Logger::Info("GraphicsDevice", "GraphicsDevice shutdown complete");
}

bool GraphicsDevice::BeginFrame() {
    if (!m_initialized || m_frameStarted) {
        return false;
    }

    // Bu frame için silme kuyruğunu temizle
    m_currentFrameIndex = m_frameManager->GetCurrentFrameIndex();
    for (auto& pair : m_deletionQueue[m_currentFrameIndex]) {
        vkDestroyBuffer(m_vulkanDevice->GetDevice(), pair.first, nullptr);
        vkFreeMemory(m_vulkanDevice->GetDevice(), pair.second, nullptr);
    }
    m_deletionQueue[m_currentFrameIndex].clear();
    
    // Frame manager'a delege et
    if (!m_frameManager || !m_frameManager->BeginFrame()) {
        return false;
    }
    
    // At the beginning of the frame's rendering commands
    if (m_vulkanRenderer) {
        m_vulkanRenderer->ResetInstanceBuffer();
    }
    
    m_frameStarted = true;
    return true;
}

bool GraphicsDevice::EndFrame() {
    if (!m_initialized || !m_frameStarted) {
        return false;
    }
    
    // Frame manager'a delege et
    if (!m_frameManager || !m_frameManager->EndFrame()) {
        return false;
    }
    
    // Transfer manager'da kuyruğa alınan transferleri gönder
    if (m_transferManager) {
        m_transferManager->SubmitTransfers();
    }
    
    m_frameStarted = false;
    m_currentFrameIndex = m_frameManager->GetCurrentFrameIndex();
    return true;
}

bool GraphicsDevice::WaitForFrame() {
    if (!m_initialized) {
        return false;
    }
    
    // Frame manager'a delege et
    return m_frameManager && m_frameManager->WaitForFrame();
}

void GraphicsDevice::RecreateSwapchain() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("GraphicsDevice", "Recreating swapchain...");
    
    // Cihazı idle bekle
    vkDeviceWaitIdle(m_vulkanDevice->GetDevice());
    
    // Swapchain'i temizle
    CleanupSwapchain();
    
    // Yeni swapchain oluştur
    if (!CreateSwapchain()) {
        Logger::Error("GraphicsDevice", "Failed to recreate swapchain");
        return;
    }
    
    // Frame manager'ı bilgilendir
    if (m_frameManager) {
        m_frameManager->RecreateSwapchain(m_swapchain.get());
    }
    
    Logger::Info("GraphicsDevice", "Swapchain recreated successfully");
}

std::string GraphicsDevice::GetDebugReport() const {
    std::ostringstream oss;
    oss << "=== GraphicsDevice Debug Report ===\n";
    oss << "Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    oss << "Validation Layers: " << (m_config.enableValidationLayers ? "Enabled" : "Disabled") << "\n";
    oss << "Timeline Semaphores: " << (m_timelineSemaphoreSupported ? "Supported" : "Not Supported") << "\n";
    oss << "Max Frames In Flight: " << m_config.maxFramesInFlight << "\n";
    oss << "Current Frame Index: " << m_currentFrameIndex << "\n";
    oss << "Frame Started: " << (m_frameStarted ? "Yes" : "No") << "\n";
    
    if (m_memoryManager) {
        oss << "\n--- Memory Manager ---\n";
        oss << m_memoryManager->GetDebugReport();
    }
    
    if (m_synchronization) {
        oss << "\n--- Synchronization Manager ---\n";
        oss << m_synchronization->GetDebugReport();
    }
    
    return oss.str();
}

void GraphicsDevice::DumpMemoryMap() const {
    if (m_memoryManager) {
        m_memoryManager->DumpMemoryMap();
    }
}

void GraphicsDevice::CheckForLeaks() const {
    if (m_memoryManager) {
        m_memoryManager->CheckForLeaks();
    }
}

#ifdef ASTRAL_USE_VULKAN

bool GraphicsDevice::CreateInstance() {
    Logger::Info("GraphicsDevice", "Creating VulkanInstance wrapper...");
    m_vulkanInstance = std::make_unique<VulkanInstance>();
    
    Logger::Info("GraphicsDevice", "Configuring VulkanInstance...");
    AstralEngine::Config instanceConfig;
    instanceConfig.applicationName = m_config.applicationName;
    instanceConfig.applicationVersion = m_config.applicationVersion;
    instanceConfig.engineName = m_config.engineName;
    instanceConfig.engineVersion = m_config.engineVersion;
    instanceConfig.apiVersion = m_config.apiVersion;
    instanceConfig.enableValidationLayers = m_config.enableValidationLayers;
    instanceConfig.enableDebugUtils = m_config.enableValidationLayers;
    
    Logger::Info("GraphicsDevice", "Validation layers: {}, Debug utils: {}",
                instanceConfig.enableValidationLayers ? "Enabled" : "Disabled",
                instanceConfig.enableDebugUtils ? "Enabled" : "Disabled");

    // Get all required extensions, including debug ones
    auto requiredExtensions_c = GetRequiredInstanceExtensions();
    instanceConfig.instanceExtensions = std::vector<std::string>(requiredExtensions_c.begin(), requiredExtensions_c.end());
    
    Logger::Info("GraphicsDevice", "Initializing VulkanInstance with {} total extensions", instanceConfig.instanceExtensions.size());
    if (!m_vulkanInstance->Initialize(instanceConfig)) {
        Logger::Error("GraphicsDevice", "Failed to initialize VulkanInstance: " + m_vulkanInstance->GetLastError());
        SetError("Failed to initialize VulkanInstance: " + m_vulkanInstance->GetLastError());
        return false;
    }
    
    Logger::Info("GraphicsDevice", "VulkanInstance initialized successfully");
    return true;
}

bool GraphicsDevice::SetupDebugMessenger() {
    Logger::Info("GraphicsDevice", "Setting up debug messenger...");
    
    if (!m_config.enableValidationLayers) {
        Logger::Info("GraphicsDevice", "Validation layers disabled - skipping debug messenger setup");
        return true;
    }
    
    Logger::Info("GraphicsDevice", "Validation layers enabled - proceeding with debug messenger setup");
    
    if (!m_vulkanInstance) {
        Logger::Error("GraphicsDevice", "Vulkan instance is null");
        SetError("Vulkan instance is null");
        return false;
    }
    
    Logger::Info("GraphicsDevice", "Vulkan instance is valid, proceeding with debug messenger setup");
    
    // Check if debug utils is enabled in the VulkanInstance
    if (!m_vulkanInstance->IsDebugUtilsEnabled()) {
        Logger::Warning("GraphicsDevice", "Debug utils not enabled in VulkanInstance - skipping debug messenger setup");
        return true; // Not a fatal error
    }

    Logger::Info("GraphicsDevice", "Debug utils enabled in VulkanInstance - proceeding with debug messenger setup");
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    VulkanUtils::PopulateDebugMessengerCreateInfo(createInfo);
    
    Logger::Info("GraphicsDevice", "Loading vkCreateDebugUtilsMessengerEXT function...");
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vulkanInstance->GetInstance(), "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr) {
        Logger::Error("GraphicsDevice", "vkCreateDebugUtilsMessengerEXT function not found");
        Logger::Warning("GraphicsDevice", "This indicates VK_EXT_DEBUG_UTILS_EXTENSION_NAME was not properly loaded");
        SetError("vkCreateDebugUtilsMessengerEXT not present");
        return false;
    }
    
    Logger::Info("GraphicsDevice", "vkCreateDebugUtilsMessengerEXT function loaded successfully");
    
    Logger::Info("GraphicsDevice", "Creating debug messenger...");
    VkResult result = func(m_vulkanInstance->GetInstance(), &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS) {
        Logger::Error("GraphicsDevice", "Failed to create debug messenger: " + VulkanUtils::GetVkResultString(result));
        Logger::Warning("GraphicsDevice", "Debug messenger creation failed with result: " + VulkanUtils::GetVkResultString(result));
        SetError("Failed to create debug messenger: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    Logger::Info("GraphicsDevice", "Debug messenger created successfully - handle: {}", static_cast<void*>(m_debugMessenger));
    return true;
}

bool GraphicsDevice::CreateSurface(Window* window) {
    return window->CreateVulkanSurface(m_vulkanInstance->GetInstance(), &m_surface);
}

bool GraphicsDevice::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_vulkanInstance->GetInstance(), &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        SetError("No Vulkan physical devices found");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_vulkanInstance->GetInstance(), &deviceCount, devices.data());
    
    // Cihazları uygunluklarına göre sırala
    std::multimap<int, VkPhysicalDevice> candidates;
    
    for (const auto& device : devices) {
        int score = RateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }
    
    // En yüksek puanlı cihazı seç
    if (candidates.rbegin()->first > 0) {
        // VulkanDevice wrapper'ını oluştur ve physical device'i ayarla
        m_vulkanDevice = std::make_unique<VulkanDevice>();
        m_vulkanDevice->SetPhysicalDevice(candidates.rbegin()->second);
        Logger::Info("GraphicsDevice", "Selected physical device with score: {}", candidates.rbegin()->first);
        return true;
    }
    
    SetError("No suitable Vulkan physical device found");
    return false;
}

bool GraphicsDevice::CreateLogicalDevice() {
    // m_vulkanDevice zaten PickPhysicalDevice'da oluşturuldu
    
    VulkanDevice::Config deviceConfig;
    deviceConfig.enableValidationLayers = m_config.enableValidationLayers;
    deviceConfig.deviceExtensions = m_deviceExtensions;
    deviceConfig.surface = m_surface;
    
    if (!m_vulkanDevice->Initialize(m_vulkanInstance.get(), m_window)) {
        SetError("Failed to initialize VulkanDevice: " + m_vulkanDevice->GetLastError());
        return false;
    }
    
    // Timeline semaphore desteğini kontrol et
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
    timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &timelineFeatures;
    
    vkGetPhysicalDeviceFeatures2(m_vulkanDevice->GetPhysicalDevice(), &deviceFeatures2);
    m_timelineSemaphoreSupported = timelineFeatures.timelineSemaphore == VK_TRUE;
    
    if (!m_timelineSemaphoreSupported && m_config.enableTimelineSemaphores) {
        Logger::Warning("GraphicsDevice", "Timeline semaphores not supported, falling back to binary semaphores");
    }
    
    return true;
}

bool GraphicsDevice::CreateSwapchain() {
    m_swapchain = std::make_unique<VulkanSwapchain>();
    
    if (!m_swapchain->Initialize(m_vulkanDevice.get())) {
        SetError("Failed to initialize VulkanSwapchain: " + m_swapchain->GetLastError());
        return false;
    }
    
    return true;
}

bool GraphicsDevice::CreateMemoryManager() {
    m_memoryManager = std::make_unique<VulkanMemoryManager>();
    
    VulkanMemoryManager::Config memoryConfig;
    memoryConfig.enableDefragmentation = true;
    memoryConfig.enableMemoryTracking = true;
    memoryConfig.enableLeakDetection = true;
    memoryConfig.enableDebugNames = m_config.enableDebugNames;
    
    if (!m_memoryManager->Initialize(m_vulkanDevice.get(), memoryConfig)) {
        SetError("Failed to initialize VulkanMemoryManager: " + m_memoryManager->GetLastError());
        return false;
    }
    
    return true;
}

bool GraphicsDevice::CreateSynchronization() {
    m_synchronization = std::make_unique<VulkanSynchronization>();
    
    VulkanSynchronization::Config syncConfig;
    syncConfig.enableTimelineSemaphores = m_timelineSemaphoreSupported && m_config.enableTimelineSemaphores;
    syncConfig.enableDebugNames = m_config.enableDebugNames;
    syncConfig.maxSemaphores = 32;
    syncConfig.maxFences = 16;
    
    if (!m_synchronization->Initialize(m_vulkanDevice.get(), syncConfig)) {
        SetError("Failed to initialize VulkanSynchronization: " + m_synchronization->GetLastError());
        return false;
    }
    
    return true;
}

bool GraphicsDevice::CreateRenderer() {
    m_vulkanRenderer = std::make_unique<VulkanRenderer>();
    
    // Check if owner is valid before passing to renderer
    if (!m_owner) {
        SetError("Cannot create renderer - engine owner is null");
        return false;
    }
    
    if (!m_vulkanRenderer->Initialize(this, m_owner)) {
        SetError("Failed to initialize VulkanRenderer: " + m_vulkanRenderer->GetLastError());
        return false;
    }
    
    return true;
}

bool GraphicsDevice::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : m_validationLayers) {
        bool layerFound = false;
        
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

bool GraphicsDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
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

std::vector<const char*> GraphicsDevice::GetRequiredInstanceExtensions() const {
    std::vector<const char*> extensions;
    
    // Window'dan gerekli extension'ları al
    auto windowExtensions = m_window->GetRequiredVulkanExtensions();
    extensions.insert(extensions.end(), windowExtensions.begin(), windowExtensions.end());
    
    // Debug extension'ı ekle
    if (m_config.enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return extensions;
}

std::vector<const char*> GraphicsDevice::GetRequiredDeviceExtensions() const {
    return m_deviceExtensions;
}

bool GraphicsDevice::IsDeviceSuitable(VkPhysicalDevice device) {
    return RateDeviceSuitability(device) > 0;
}

int GraphicsDevice::RateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    int score = 0;
    
    // Discrete GPU'lar entegre GPU'lardan daha iyi
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    
    // Maksimum image boyutu önemli
    score += deviceProperties.limits.maxImageDimension2D;
    
    // Texture compression özelliği önemli
    if (deviceFeatures.textureCompressionBC) {
        score += 100;
    }
    
    // Geometry shader desteği bonus
    if (deviceFeatures.geometryShader) {
        score += 50;
    }
    
    // Extension desteğini kontrol et
    if (!CheckDeviceExtensionSupport(device)) {
        return 0;
    }
    
    // Swap chain desteğini kontrol et - geçici olarak basit kontrol
    // m_vulkanDevice henüz initialize edilmediği için doğrudan kontrol ediyoruz
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount == 0) {
        return 0;
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
        return 0;
    }
    
    return score;
}

void GraphicsDevice::LogInitialization() const {
    Logger::Info("GraphicsDevice", "=== Initialization Summary ===");
    Logger::Info("GraphicsDevice", "Application: {} v{}", m_config.applicationName, 
                VK_VERSION_MAJOR(m_config.applicationVersion));
    Logger::Info("GraphicsDevice", "Engine: {} v{}", m_config.engineName, 
                VK_VERSION_MAJOR(m_config.engineVersion));
    Logger::Info("GraphicsDevice", "Vulkan API: v{}.{}.{}", 
                VK_VERSION_MAJOR(m_config.apiVersion),
                VK_VERSION_MINOR(m_config.apiVersion),
                VK_VERSION_PATCH(m_config.apiVersion));
    Logger::Info("GraphicsDevice", "Validation Layers: {}", m_config.enableValidationLayers ? "Enabled" : "Disabled");
    Logger::Info("GraphicsDevice", "Timeline Semaphores: {}", m_timelineSemaphoreSupported ? "Supported" : "Not Supported");
    Logger::Info("GraphicsDevice", "Max Frames In Flight: {}", m_config.maxFramesInFlight);
}

void GraphicsDevice::LogDeviceCapabilities() const {
    if (!m_vulkanDevice) {
        return;
    }
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_vulkanDevice->GetPhysicalDevice(), &properties);
    
    Logger::Info("GraphicsDevice", "=== Device Capabilities ===");
    Logger::Info("GraphicsDevice", "Device Name: {}", properties.deviceName);
    Logger::Info("GraphicsDevice", "Device Type: {}", 
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" :
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated GPU" :
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual GPU" : "Other");
    Logger::Info("GraphicsDevice", "Driver Version: {}", properties.driverVersion);
    Logger::Info("GraphicsDevice", "Vulkan API Version: {}.{}.{}", 
                VK_VERSION_MAJOR(properties.apiVersion),
                VK_VERSION_MINOR(properties.apiVersion),
                VK_VERSION_PATCH(properties.apiVersion));
    Logger::Info("GraphicsDevice", "Max Image Dimension 2D: {}", properties.limits.maxImageDimension2D);
    Logger::Info("GraphicsDevice", "Max Framebuffer Width: {}", properties.limits.maxFramebufferWidth);
    Logger::Info("GraphicsDevice", "Max Framebuffer Height: {}", properties.limits.maxFramebufferHeight);
}

void GraphicsDevice::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("GraphicsDevice", error);
}

void GraphicsDevice::ClearError() {
    m_lastError.clear();
}

void GraphicsDevice::CleanupSwapchain() {
    if (m_swapchain) {
        m_swapchain->Shutdown();
        m_swapchain.reset();
    }
}

void GraphicsDevice::QueueBufferForDeletion(VkBuffer buffer, VkDeviceMemory memory) {
    if (m_initialized) {
        m_deletionQueue[m_currentFrameIndex].push_back({buffer, memory});
    }
}

bool GraphicsDevice::CreateDescriptorSetLayout() {
    // UBO binding
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Texture sampler binding
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_vulkanDevice->GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        SetError("Failed to create descriptor set layout");
        return false;
    }

    return true;
}

#endif // ASTRAL_USE_VULKAN

} // namespace AstralEngine
