#include "VulkanGraphicsContext.h"
#include "../Core/VulkanInstance.h"
#include "../Core/VulkanDevice.h"
#include "../Core/VulkanSwapchain.h"
#include "../../Platform/PlatformSubsystem.h"
#include "../../../Core/Logger.h"
#include <iostream>

namespace AstralEngine {

VulkanGraphicsContext::VulkanGraphicsContext() 
    : m_owner(nullptr)
    , m_isInitialized(false) {
}

VulkanGraphicsContext::~VulkanGraphicsContext() {
    if (m_isInitialized) {
        Shutdown();
    }
}

bool VulkanGraphicsContext::Initialize(Engine* owner, const VulkanGraphicsContextConfig& config) {
    if (m_isInitialized) {
        SetLastError("VulkanGraphicsContext already initialized");
        return false;
    }
    
    try {
        m_owner = owner;
        m_config = config;
        
        // Initialize core components
        if (!InitializeInstance()) {
            return false;
        }
        
        if (!InitializeDevice()) {
            return false;
        }
        
        if (!InitializeSwapchain()) {
            return false;
        }
        
        m_isInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanGraphicsContext::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    try {
        // Shutdown in reverse order of initialization
        if (m_swapchain) {
            m_swapchain->Shutdown();
            m_swapchain.reset();
        }
        
        if (m_device) {
            m_device->Shutdown(m_instance->GetInstance());
            m_device.reset();
        }
        
        if (m_instance) {
            m_instance->Shutdown();
            m_instance.reset();
        }
        
        m_isInitialized = false;
        m_owner = nullptr;
    }
    catch (const std::exception& e) {
        SetLastError("Shutdown failed: " + std::string(e.what()));
    }
}

VkInstance VulkanGraphicsContext::GetVkInstance() const {
    return m_instance ? m_instance->GetInstance() : VK_NULL_HANDLE;
}

VkDevice VulkanGraphicsContext::GetVkDevice() const {
    return m_device ? m_device->GetDevice() : VK_NULL_HANDLE;
}

VkPhysicalDevice VulkanGraphicsContext::GetVkPhysicalDevice() const {
    return m_device ? m_device->GetPhysicalDevice() : VK_NULL_HANDLE;
}

VkQueue VulkanGraphicsContext::GetGraphicsQueue() const {
    return m_device ? m_device->GetGraphicsQueue() : VK_NULL_HANDLE;
}

VkQueue VulkanGraphicsContext::GetPresentQueue() const {
    return m_device ? m_device->GetPresentQueue() : VK_NULL_HANDLE;
}

void VulkanGraphicsContext::UpdateConfig(const VulkanGraphicsContextConfig& config) {
    m_config = config;
    // TODO: Reinitialize components if configuration changes significantly
}

bool VulkanGraphicsContext::RecreateSwapchain() {
    if (!m_swapchain || !m_isInitialized) {
        SetLastError("Cannot recreate swapchain - not initialized");
        return false;
    }
    
    Logger::Info("VulkanGraphicsContext", "Starting swapchain recreation...");
    
    try {
        // Wait for device to finish current operations before recreating swapchain
        Logger::Debug("VulkanGraphicsContext", "Waiting for device idle before swapchain recreation...");
        if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device->GetDevice());
        }
        
        // Recreate the swapchain
        Logger::Info("VulkanGraphicsContext", "Recreating Vulkan swapchain...");
        if (!m_swapchain->Recreate()) {
            SetLastError("Failed to recreate swapchain");
            return false;
        }
        
        Logger::Info("VulkanGraphicsContext", "Swapchain recreated successfully");
        Logger::Info("VulkanGraphicsContext", "New swapchain extent: {}x{}",
                     m_swapchain->GetExtent().width, m_swapchain->GetExtent().height);
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Swapchain recreation failed: " + std::string(e.what()));
        Logger::Error("VulkanGraphicsContext", "Swapchain recreation exception: {}", e.what());
        return false;
    }
}

VkExtent2D VulkanGraphicsContext::GetSwapchainExtent() const {
    return m_swapchain ? m_swapchain->GetExtent() : VkExtent2D{0, 0};
}

uint32_t VulkanGraphicsContext::GetSwapchainImageCount() const {
    return m_swapchain ? m_swapchain->GetImageCount() : 0;
}

bool VulkanGraphicsContext::InitializeInstance() {
    try {
        m_instance = std::make_unique<VulkanInstance>();
        Config instanceConfig;
        instanceConfig.applicationName = m_config.applicationName;
        instanceConfig.applicationVersion = m_config.applicationVersion;
        instanceConfig.engineName = m_config.engineName;
        instanceConfig.engineVersion = m_config.engineVersion;
        instanceConfig.apiVersion = VK_API_VERSION_1_4;
        instanceConfig.enableValidationLayers = m_config.enableValidationLayers;
        instanceConfig.enableDebugUtils = true;
        
        // Add SDL3 required extensions manually
        instanceConfig.instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32
        instanceConfig.instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
        instanceConfig.instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
        
        std::cout << "[VulkanGraphicsContext] Added SDL3 extension: " << VK_KHR_SURFACE_EXTENSION_NAME << std::endl;
#ifdef _WIN32
        std::cout << "[VulkanGraphicsContext] Added SDL3 extension: " << VK_KHR_WIN32_SURFACE_EXTENSION_NAME << std::endl;
#else
        std::cout << "[VulkanGraphicsContext] Added SDL3 extension: " << VK_KHR_XCB_SURFACE_EXTENSION_NAME << std::endl;
#endif
        
        if (!m_instance->Initialize(instanceConfig)) {
            SetLastError("Failed to initialize Vulkan instance: " + m_instance->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanGraphicsContext", "Vulkan instance initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Instance initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanGraphicsContext::InitializeDevice() {
    Logger::Info("VulkanGraphicsContext", "Initializing Vulkan device...");
    
    try {
        m_device = std::make_unique<VulkanDevice>();
        VulkanDevice::Config deviceConfig;
        deviceConfig.enableValidationLayers = m_config.enableValidationLayers;
        
        // Get window from engine
        Logger::Info("VulkanGraphicsContext", "Getting PlatformSubsystem from engine...");
        auto* platformSubsystem = m_owner->GetSubsystem<PlatformSubsystem>();
        if (!platformSubsystem) {
            SetLastError("PlatformSubsystem not found");
            Logger::Error("VulkanGraphicsContext", "PlatformSubsystem not found - this will cause initialization failure");
            return false;
        }
        Logger::Info("VulkanGraphicsContext", "PlatformSubsystem found: {}", reinterpret_cast<uintptr_t>(platformSubsystem));
        
        auto* window = platformSubsystem->GetWindow();
        if (!window) {
            SetLastError("Window not found");
            Logger::Error("VulkanGraphicsContext", "Window not found - this will cause initialization failure");
            return false;
        }
        Logger::Info("VulkanGraphicsContext", "Window found: {}", reinterpret_cast<uintptr_t>(window));
        
        Logger::Info("VulkanGraphicsContext", "Calling VulkanDevice::Initialize...");
        if (!m_device->Initialize(m_instance.get(), window)) {
            SetLastError("Failed to initialize Vulkan device: " + m_device->GetLastError());
            Logger::Error("VulkanGraphicsContext", "VulkanDevice initialization failed: {}", m_device->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanGraphicsContext", "Vulkan device initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Device initialization failed: " + std::string(e.what()));
        Logger::Error("VulkanGraphicsContext", "Device initialization failed with exception: {}", e.what());
        return false;
    }
}

bool VulkanGraphicsContext::InitializeSwapchain() {
    try {
        m_swapchain = std::make_unique<VulkanSwapchain>();
        if (!m_swapchain->Initialize(m_device.get())) {
            SetLastError("Failed to initialize Vulkan swapchain");
            return false;
        }
        
        Logger::Info("VulkanGraphicsContext", "Vulkan swapchain initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Swapchain initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanGraphicsContext::SetLastError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanGraphicsContext", error);
}

} // namespace AstralEngine
