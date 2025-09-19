#include "VulkanFramebuffer.h"
#include <algorithm>

namespace AstralEngine {

VulkanFramebuffer::VulkanFramebuffer() 
    : m_framebuffer(VK_NULL_HANDLE)
    , m_width(0)
    , m_height(0)
    , m_layers(1)
    , m_isInitialized(false) {
    
    // Set default configuration
    m_config = Config{};
}

VulkanFramebuffer::~VulkanFramebuffer() {
    if (m_isInitialized) {
        // Note: We can't call Shutdown here without the device
        // This should be handled by the owner
        // For now, just log a warning
        Logger::Warning("VulkanFramebuffer", "VulkanFramebuffer destroyed without proper shutdown");
    }
}

bool VulkanFramebuffer::Initialize(const Config& config) {
    if (m_isInitialized) {
        Logger::Error("VulkanFramebuffer", "VulkanFramebuffer already initialized");
        return false;
    }
    
    if (!config.device) {
        SetError("Invalid VulkanDevice pointer");
        return false;
    }
    
    if (config.renderPass == VK_NULL_HANDLE) {
        SetError("Invalid render pass handle");
        return false;
    }
    
    if (config.attachments.empty()) {
        SetError("No attachments provided");
        return false;
    }
    
    if (config.width == 0 || config.height == 0) {
        SetError("Invalid framebuffer dimensions");
        return false;
    }
    
    if (config.layers == 0) {
        SetError("Invalid layer count");
        return false;
    }
    
    try {
        Logger::Info("VulkanFramebuffer", "Initializing VulkanFramebuffer...");
        Logger::Info("VulkanFramebuffer", "Framebuffer dimensions: " + std::to_string(config.width) + "x" + std::to_string(config.height));
        Logger::Info("VulkanFramebuffer", "Attachment count: " + std::to_string(config.attachments.size()));
        Logger::Info("VulkanFramebuffer", "Layer count: " + std::to_string(config.layers));
        
        // Store configuration
        m_config = config;
        m_width = config.width;
        m_height = config.height;
        m_layers = config.layers;
        m_attachments = config.attachments;
        
        // Create framebuffer info
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = config.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(config.attachments.size());
        framebufferInfo.pAttachments = config.attachments.data();
        framebufferInfo.width = config.width;
        framebufferInfo.height = config.height;
        framebufferInfo.layers = config.layers;
        
        // Create framebuffer
        VkResult result = vkCreateFramebuffer(config.device->GetDevice(), &framebufferInfo, nullptr, &m_framebuffer);
        if (result != VK_SUCCESS) {
            HandleVulkanError(result, "create framebuffer");
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanFramebuffer", "VulkanFramebuffer initialized successfully");
        Logger::Info("VulkanFramebuffer", "Framebuffer handle: " + std::to_string(reinterpret_cast<uintptr_t>(m_framebuffer)));
        
        return true;
    }
    catch (const std::exception& e) {
        SetError("Initialization failed: " + std::string(e.what()));
        Logger::Error("VulkanFramebuffer", "VulkanFramebuffer initialization failed: " + GetLastError());
        return false;
    }
}

void VulkanFramebuffer::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    try {
        Logger::Info("VulkanFramebuffer", "Shutting down VulkanFramebuffer...");
        
        // Destroy framebuffer
        if (m_framebuffer != VK_NULL_HANDLE && m_config.device) {
            vkDestroyFramebuffer(m_config.device->GetDevice(), m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
        }
        
        // Reset properties
        m_width = 0;
        m_height = 0;
        m_layers = 1;
        m_attachments.clear();
        
        m_isInitialized = false;
        Logger::Info("VulkanFramebuffer", "VulkanFramebuffer shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanFramebuffer", "VulkanFramebuffer shutdown failed: " + std::string(e.what()));
    }
}

void VulkanFramebuffer::HandleVulkanError(VkResult result, const std::string& operation) {
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
        case VK_ERROR_MEMORY_MAP_FAILED:
            errorString = "Memory map failed";
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            errorString = "Layer not present";
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            errorString = "Extension not present";
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            errorString = "Feature not present";
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            errorString = "Incompatible driver";
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

void VulkanFramebuffer::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanFramebuffer", error);
}

} // namespace AstralEngine