#include "VulkanCommandPool.h"
#include "Core/Logger.h"

namespace AstralEngine {

VulkanCommandPool::VulkanCommandPool() 
    : m_device(nullptr)
    , m_commandPool(VK_NULL_HANDLE)
    , m_isInitialized(false) {
    
    // Set default configuration
    m_config = Config{};
}

VulkanCommandPool::~VulkanCommandPool() {
    if (m_isInitialized) {
        Shutdown();
    }
}

bool VulkanCommandPool::Initialize(VulkanDevice* device, const Config& config) {
    if (m_isInitialized) {
        SetError("Command pool already initialized");
        return false;
    }

    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }

    try {
        Logger::Info("VulkanCommandPool", "Initializing Vulkan command pool...");
        
        m_device = device;
        m_config = config;

        // Create command pool create info
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = m_config.flags;
        poolInfo.queueFamilyIndex = m_config.queueFamilyIndex;

        // Create command pool
        VkResult result = vkCreateCommandPool(m_device->GetDevice(), &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkCreateCommandPool");
            return false;
        }

        m_isInitialized = true;
        Logger::Info("VulkanCommandPool", "Vulkan command pool initialized successfully");
        return true;

    } catch (const std::exception& e) {
        SetError("Command pool initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanCommandPool::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    try {
        Logger::Info("VulkanCommandPool", "Shutting down Vulkan command pool...");

        // Destroy command pool
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device->GetDevice(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_device = nullptr;
        m_isInitialized = false;
        Logger::Info("VulkanCommandPool", "Vulkan command pool shutdown completed");

    } catch (const std::exception& e) {
        Logger::Error("VulkanCommandPool", "Command pool shutdown failed: " + std::string(e.what()));
    }
}

bool VulkanCommandPool::AllocateCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers) {
    if (!m_isInitialized) {
        SetError("Command pool not initialized");
        return false;
    }

    if (count == 0 || !commandBuffers) {
        SetError("Invalid parameters for command buffer allocation");
        return false;
    }

    try {
        // Create command buffer allocate info
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;

        // Allocate command buffers
        VkResult result = vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, commandBuffers);
        if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkAllocateCommandBuffers");
            return false;
        }

        Logger::Debug("VulkanCommandPool", "Allocated {} command buffers", count);
        return true;

    } catch (const std::exception& e) {
        SetError("Command buffer allocation failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanCommandPool::FreeCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers) {
    if (!m_isInitialized || !commandBuffers || count == 0) {
        return;
    }

    try {
        // Free command buffers
        vkFreeCommandBuffers(m_device->GetDevice(), m_commandPool, count, commandBuffers);
        Logger::Debug("VulkanCommandPool", "Freed {} command buffers", count);

    } catch (const std::exception& e) {
        Logger::Error("VulkanCommandPool", "Command buffer free failed: " + std::string(e.what()));
    }
}

void VulkanCommandPool::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanCommandPool", error);
}

void VulkanCommandPool::HandleVulkanError(VkResult result, const std::string& operation) {
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
        default:
            errorString = "Unknown Vulkan error (" + std::to_string(result) + ")";
            break;
    }
    
    SetError("Vulkan error during " + operation + ": " + errorString);
}

} // namespace AstralEngine
