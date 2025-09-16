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
    
    // YENİ: Frame kaynaklarını oluştur (Renderer'dan ÖNCE)
    Logger::Info("GraphicsDevice", "Creating frame resources...");
    
    if (!CreateDescriptorSetLayout()) {
        Logger::Error("GraphicsDevice", "Failed to create descriptor set layout");
        return false;
    }
    
    if (!CreateDescriptorPool()) {
        Logger::Error("GraphicsDevice", "Failed to create descriptor pool");
        return false;
    }
    
    if (!CreateUniformBuffers()) {
        Logger::Error("GraphicsDevice", "Failed to create uniform buffers");
        return false;
    }
    
    if (!CreateDescriptorSets()) {
        Logger::Error("GraphicsDevice", "Failed to create descriptor sets");
        return false;
    }
    
    if (!UpdateDescriptorSets()) {
        Logger::Error("GraphicsDevice", "Failed to update descriptor sets");
        return false;
    }
    
    if (!CreateCommandBuffers()) {
        Logger::Error("GraphicsDevice", "Failed to create command buffers");
        return false;
    }

    if (!CreateRenderer()) {
        Logger::Error("GraphicsDevice", "Failed to create renderer");
        return false;
    }
    
    // Frame senkronizasyon nesnelerini oluştur
    m_imageAvailableSemaphores.resize(m_config.maxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_config.maxFramesInFlight);
    m_inFlightFences.resize(m_config.maxFramesInFlight);
    m_imagesInFlight.resize(m_swapchain->GetImageCount(), VK_NULL_HANDLE);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (uint32_t i = 0; i < m_config.maxFramesInFlight; i++) {
        if (vkCreateSemaphore(m_vulkanDevice->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_vulkanDevice->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_vulkanDevice->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to create synchronization objects for frame {}", i);
            return false;
        }
    }
    
    Logger::Info("GraphicsDevice", "Frame resources created successfully");
    
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
    
    // Frame senkronizasyon nesnelerini temizle
    for (uint32_t i = 0; i < m_config.maxFramesInFlight; i++) {
        vkDestroySemaphore(m_vulkanDevice->GetDevice(), m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_vulkanDevice->GetDevice(), m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_vulkanDevice->GetDevice(), m_inFlightFences[i], nullptr);
    }
    
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
    m_imagesInFlight.clear();
    
    // YENİ: Frame kaynaklarını temizle
    CleanupFrameResources();
    
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
    
    // Önceki frame'in bitmesini bekle
    WaitForFrame();
    
    // Bir sonraki image'i al
    if (!AcquireNextImage()) {
        return false;
    }
    
    m_frameStarted = true;
    return true;
}

bool GraphicsDevice::EndFrame() {
    if (!m_initialized || !m_frameStarted) {
        return false;
    }
    
    // Command buffer'ı submit et
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrameIndex]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    // VulkanRenderer'dan command buffer'ı al
    VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrameIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    // Fence'i resetle ve submit et
    vkResetFences(m_vulkanDevice->GetDevice(), 1, &m_inFlightFences[m_currentFrameIndex]);
    
    VkResult result = vkQueueSubmit(m_vulkanDevice->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrameIndex]);
    if (result != VK_SUCCESS) {
        SetError("Failed to submit draw command buffer: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    // Image'i present et
    if (!PresentImage()) {
        return false;
    }
    
    // CRITICAL FIX: Command buffer'ı bir sonraki kullanım için reset et
    // Bu, VulkanRenderer'da RecordCommandBuffer çağrılmadan önce yapılmalı
    VkCommandBufferResetFlags resetFlags = 0;
    result = vkResetCommandBuffer(m_commandBuffers[m_currentFrameIndex], resetFlags);
    if (result != VK_SUCCESS) {
        Logger::Warning("GraphicsDevice", "Failed to reset command buffer {}: {}", m_currentFrameIndex, VulkanUtils::GetVkResultString(result));
        // Bu kritik bir hata değil, sadece warning logla
    }
    
    m_frameStarted = false;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_config.maxFramesInFlight;
    
    return true;
}

bool GraphicsDevice::WaitForFrame() {
    if (!m_initialized) {
        return false;
    }
    
    VkResult result = vkWaitForFences(m_vulkanDevice->GetDevice(), 1, &m_inFlightFences[m_currentFrameIndex], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        SetError("Failed to wait for fence: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
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
    
    // Images in flight vektörünü yeniden boyutlandır
    m_imagesInFlight.resize(m_swapchain->GetImageCount(), VK_NULL_HANDLE);
    
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

bool GraphicsDevice::AcquireNextImage() {
    VkResult result = vkAcquireNextImageKHR(
        m_vulkanDevice->GetDevice(),
        m_swapchain->GetSwapchain(),
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrameIndex],
        VK_NULL_HANDLE,
        &m_imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        SetError("Failed to acquire swap chain image: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    // Boundary check: m_imageIndex değerini doğrula
    if (m_imageIndex >= m_imagesInFlight.size()) {
        Logger::Error("GraphicsDevice", "Image index {} out of range for imagesInFlight vector (size: {})", 
                    m_imageIndex, m_imagesInFlight.size());
        Logger::Error("GraphicsDevice", "This indicates a swapchain synchronization issue");
        return false;
    }
    
    // Boundary check: m_currentFrameIndex değerini doğrula
    if (m_currentFrameIndex >= m_inFlightFences.size()) {
        Logger::Error("GraphicsDevice", "Current frame index {} out of range for inFlightFences vector (size: {})", 
                    m_currentFrameIndex, m_inFlightFences.size());
        return false;
    }
    
    // Eğer bu image için zaten bir fence varsa bekle
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_vulkanDevice->GetDevice(), 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Mevcut frame'in fence'ini işaretle
    m_imagesInFlight[m_imageIndex] = m_inFlightFences[m_currentFrameIndex];
    
    return true;
}

bool GraphicsDevice::PresentImage() {
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrameIndex]};
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {m_swapchain->GetSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_imageIndex;
    
    VkResult result = vkQueuePresentKHR(m_vulkanDevice->GetPresentQueue(), &presentInfo);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
        return false;
    } else if (result != VK_SUCCESS) {
        SetError("Failed to present swap chain image: " + VulkanUtils::GetVkResultString(result));
        return false;
    }
    
    return true;
}

void GraphicsDevice::CleanupSwapchain() {
    if (m_swapchain) {
        m_swapchain->Shutdown();
        m_swapchain.reset();
    }
}

bool GraphicsDevice::CreateCommandBuffers() {
    Logger::Info("GraphicsDevice", "Creating command buffers");
    
    try {
        // Create command pool first
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = GetGraphicsQueueFamily();
        
        VkResult result = vkCreateCommandPool(GetDevice(), &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to create command pool: {}", static_cast<int32_t>(result));
            return false;
        }
        
        // Resize command buffers array
        m_commandBuffers.resize(m_config.maxFramesInFlight);
        
        // Command buffer allocation info
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_config.maxFramesInFlight);
        
        // Allocate command buffers
        result = vkAllocateCommandBuffers(GetDevice(), &allocInfo, m_commandBuffers.data());
        if (result != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to allocate command buffers: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Info("GraphicsDevice", "Command buffers allocated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Command buffers creation failed: {}", e.what());
        return false;
    }
}

bool GraphicsDevice::CreateDescriptorSetLayout() {
    Logger::Info("GraphicsDevice", "Creating descriptor set layout");
    
    try {
        // UBO binding definition
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;
        
        VkResult result = vkCreateDescriptorSetLayout(GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout);
        
        if (result != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to create descriptor set layout: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("GraphicsDevice", "Descriptor set layout created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Descriptor set layout creation failed: {}", e.what());
        return false;
    }
}

bool GraphicsDevice::CreateDescriptorPool() {
    Logger::Info("GraphicsDevice", "Creating descriptor pool");
    
    try {
        // UBO descriptors pool size
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(m_config.maxFramesInFlight);
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(m_config.maxFramesInFlight);
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        
        VkResult result = vkCreateDescriptorPool(GetDevice(), &poolInfo, nullptr, &m_descriptorPool);
        
        if (result != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to create descriptor pool: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("GraphicsDevice", "Descriptor pool created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Descriptor pool creation failed: {}", e.what());
        return false;
    }
}

bool GraphicsDevice::CreateDescriptorSets() {
    Logger::Info("GraphicsDevice", "Creating descriptor sets");
    
    try {
        // Descriptor set layout info
        std::vector<VkDescriptorSetLayout> layouts(m_config.maxFramesInFlight, m_descriptorSetLayout);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(m_config.maxFramesInFlight);
        allocInfo.pSetLayouts = layouts.data();
        
        // Allocate descriptor sets
        m_descriptorSets.resize(m_config.maxFramesInFlight);
        VkResult result = vkAllocateDescriptorSets(GetDevice(), &allocInfo, m_descriptorSets.data());
        
        if (result != VK_SUCCESS) {
            Logger::Error("GraphicsDevice", "Failed to allocate descriptor sets: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("GraphicsDevice", "Descriptor sets allocated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Descriptor sets creation failed: {}", e.what());
        return false;
    }
}

bool GraphicsDevice::CreateUniformBuffers() {
    Logger::Info("GraphicsDevice", "Creating uniform buffers");
    
    try {
        // Uniform Buffer Object size
        VkDeviceSize bufferSize = sizeof(VulkanRenderer::UniformBufferObject);
        
        // Create uniform buffer for each frame
        m_uniformBuffers.resize(m_config.maxFramesInFlight);
        
        for (size_t i = 0; i < m_config.maxFramesInFlight; i++) {
            m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
            
            VulkanBuffer::Config bufferConfig;
            bufferConfig.size = bufferSize;
            bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            
            if (!m_uniformBuffers[i]->Initialize(GetVulkanDevice(), bufferConfig)) {
                Logger::Error("GraphicsDevice", "Failed to initialize uniform buffer {}: {}", i, m_uniformBuffers[i]->GetLastError());
                return false;
            }
            
            Logger::Debug("GraphicsDevice", "Uniform buffer {} created successfully", i);
        }
        
        Logger::Debug("GraphicsDevice", "All uniform buffers created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Uniform buffers creation failed: {}", e.what());
        return false;
    }
}

bool GraphicsDevice::UpdateDescriptorSets() {
    Logger::Info("GraphicsDevice", "Updating descriptor sets");
    
    try {
        for (size_t i = 0; i < m_config.maxFramesInFlight; i++) {
            // Buffer info
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i]->GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(VulkanRenderer::UniformBufferObject);
            
            // Write descriptor set
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;
            
            vkUpdateDescriptorSets(GetDevice(), 1, &descriptorWrite, 0, nullptr);
        }
        
        Logger::Debug("GraphicsDevice", "Descriptor sets updated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Descriptor sets update failed: {}", e.what());
        return false;
    }
}

void GraphicsDevice::CleanupFrameResources() {
    Logger::Info("GraphicsDevice", "Cleaning up frame resources");
    
    try {
        // Wait for device to finish operations
        if (GetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(GetDevice());
        }
        
        // Destroy command pool
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        
        // Shutdown uniform buffers
        for (auto& buffer : m_uniformBuffers) {
            if (buffer) {
                buffer->Shutdown();
                buffer.reset();
            }
        }
        m_uniformBuffers.clear();
        
        // Destroy descriptor pool
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(GetDevice(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        
        // Destroy descriptor set layout
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        // Clear vectors
        m_commandBuffers.clear();
        m_descriptorSets.clear();
        
        Logger::Info("GraphicsDevice", "Frame resources cleanup completed");
    } catch (const std::exception& e) {
        Logger::Error("GraphicsDevice", "Frame resources cleanup failed: {}", e.what());
    }
}

#endif // ASTRAL_USE_VULKAN

} // namespace AstralEngine
