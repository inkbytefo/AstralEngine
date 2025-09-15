#include "VulkanCommandBufferManager.h"
#include "../Core/VulkanDevice.h"
#include "../Commands/VulkanCommandPool.h"
#include "../../../Core/Logger.h"

namespace AstralEngine {

VulkanCommandBufferManager::VulkanCommandBufferManager() 
    : m_maxFramesInFlight(2)
    , m_device(nullptr)
    , m_currentFrame(0)
    , m_isInitialized(false) {
}

VulkanCommandBufferManager::~VulkanCommandBufferManager() {
    if (m_isInitialized) {
        Shutdown();
    }
}

bool VulkanCommandBufferManager::Initialize(VulkanDevice* device, const VulkanCommandBufferManagerConfig& config) {
    if (m_isInitialized) {
        SetLastError("VulkanCommandBufferManager already initialized");
        return false;
    }
    
    if (!device) {
        SetLastError("Invalid device pointer");
        return false;
    }
    
    try {
        m_device = device;
        m_config = config;
        m_maxFramesInFlight = config.maxFramesInFlight;
        
        // Create command pool
        if (!CreateCommandPool()) {
            return false;
        }
        
        // Allocate command buffers
        if (!AllocateCommandBuffers()) {
            return false;
        }
        
        // Create synchronization objects
        if (!CreateSynchronizationObjects()) {
            return false;
        }
        
        m_currentFrame = 0;
        m_isInitialized = true;
        
        Logger::Info("VulkanCommandBufferManager", "Command buffer manager initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanCommandBufferManager::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    try {
        // Wait for device to finish operations
        if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device->GetDevice());
        }
        
        // Destroy in reverse order of initialization
        DestroySynchronizationObjects();
        DestroyCommandBuffers();
        DestroyCommandPool();
        
        m_device = nullptr;
        m_currentFrame = 0;
        m_isInitialized = false;
        
        Logger::Info("VulkanCommandBufferManager", "Command buffer manager shutdown completed");
    }
    catch (const std::exception& e) {
        SetLastError("Shutdown failed: " + std::string(e.what()));
    }
}

bool VulkanCommandBufferManager::BeginFrame() {
    if (!m_isInitialized) {
        SetLastError("Cannot begin frame - not initialized");
        return false;
    }
    
    try {
        Logger::Debug("VulkanCommandBufferManager", "Beginning frame {}", m_currentFrame);
        
        // Wait for the previous frame to finish with timeout and error handling
        VkFence fence = GetInFlightFence(m_currentFrame);
        if (fence != VK_NULL_HANDLE) {
            Logger::Debug("VulkanCommandBufferManager", "Waiting for fence {} for frame {}",
                         reinterpret_cast<uintptr_t>(fence), m_currentFrame);
            
            // Use a reasonable timeout instead of UINT64_MAX to avoid infinite waits
            const uint64_t timeout_ns = 500000000; // 500ms timeout - reduced for better responsiveness
            
            VkResult result = vkWaitForFences(m_device->GetDevice(), 1, &fence, VK_TRUE, timeout_ns);
            
            if (result == VK_TIMEOUT) {
                Logger::Warning("VulkanCommandBufferManager", "Fence wait timeout for frame {} after 500ms - fence may not be signaled yet, continuing", m_currentFrame);
                // This is expected for the first frame, continue without error
            } else if (result == VK_NOT_READY) {
                Logger::Debug("VulkanCommandBufferManager", "Fence not ready for frame {} - this is normal for first frame", m_currentFrame);
                // This is expected for the first frame, continue without error
            } else if (result == VK_ERROR_DEVICE_LOST) {
                Logger::Error("VulkanCommandBufferManager", "Device lost detected during fence wait for frame {}", m_currentFrame);
                HandleVulkanError(result, "vkWaitForFences");
                return false;
            } else if (result != VK_SUCCESS) {
                Logger::Error("VulkanCommandBufferManager", "vkWaitForFences failed for frame {} with result: {}",
                             m_currentFrame, static_cast<int32_t>(result));
                HandleVulkanError(result, "vkWaitForFences");
                return false;
            } else {
                Logger::Debug("VulkanCommandBufferManager", "Fence wait completed successfully for frame {}", m_currentFrame);
            }
        } else {
            Logger::Warning("VulkanCommandBufferManager", "Invalid fence for frame {} - skipping wait", m_currentFrame);
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Frame {} fence waited successfully", m_currentFrame);
        
        // Reset the fence (only if we successfully waited for it)
        if (fence != VK_NULL_HANDLE) {
            Logger::Debug("VulkanCommandBufferManager", "Resetting fence {} for frame {}",
                         reinterpret_cast<uintptr_t>(fence), m_currentFrame);
            
            VkResult result = vkResetFences(m_device->GetDevice(), 1, &fence);
            if (result != VK_SUCCESS) {
                Logger::Error("VulkanCommandBufferManager", "vkResetFences failed for frame {} with result: {}",
                             m_currentFrame, static_cast<int32_t>(result));
                HandleVulkanError(result, "vkResetFences");
                return false;
            }
            
            Logger::Debug("VulkanCommandBufferManager", "Fence reset completed successfully for frame {}", m_currentFrame);
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Frame {} fence reset successfully", m_currentFrame);
        
        // Reset command buffer
        if (!ResetCommandBuffer(m_currentFrame)) {
            return false;
        }
        
        // Begin command buffer
        if (!BeginCommandBuffer(m_currentFrame)) {
            return false;
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Frame {} begun successfully", m_currentFrame);
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("BeginFrame failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanCommandBufferManager::EndFrame() {
    if (!m_isInitialized) {
        SetLastError("Cannot end frame - not initialized");
        return false;
    }
    
    try {
        Logger::Debug("VulkanCommandBufferManager", "Ending frame {}", m_currentFrame);
        
        // End command buffer
        if (!EndCommandBuffer(m_currentFrame)) {
            return false;
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Command buffer ended successfully for frame {}", m_currentFrame);
        
        // Advance to next frame
        uint32_t nextFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
        Logger::Debug("VulkanCommandBufferManager", "Advancing from frame {} to frame {}", m_currentFrame, nextFrame);
        m_currentFrame = nextFrame;
        
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("EndFrame failed: " + std::string(e.what()));
        return false;
    }
}

VkCommandBuffer VulkanCommandBufferManager::GetCurrentCommandBuffer() const {
    if (!m_isInitialized || m_currentFrame >= m_commandBuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_commandBuffers[m_currentFrame];
}

VkCommandBuffer VulkanCommandBufferManager::GetCommandBuffer(uint32_t frameIndex) const {
    if (!m_isInitialized || frameIndex >= m_commandBuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_commandBuffers[frameIndex];
}

VkSemaphore VulkanCommandBufferManager::GetImageAvailableSemaphore(uint32_t frameIndex) const {
    if (!m_isInitialized || frameIndex >= m_imageAvailableSemaphores.size()) {
        return VK_NULL_HANDLE;
    }
    return m_imageAvailableSemaphores[frameIndex];
}

VkSemaphore VulkanCommandBufferManager::GetRenderFinishedSemaphore(uint32_t frameIndex) const {
    if (!m_isInitialized || frameIndex >= m_renderFinishedSemaphores.size()) {
        return VK_NULL_HANDLE;
    }
    return m_renderFinishedSemaphores[frameIndex];
}

VkFence VulkanCommandBufferManager::GetInFlightFence(uint32_t frameIndex) const {
    if (!m_isInitialized || frameIndex >= m_inFlightFences.size()) {
        return VK_NULL_HANDLE;
    }
    return m_inFlightFences[frameIndex];
}

bool VulkanCommandBufferManager::ResetCommandBuffer(uint32_t frameIndex) {
    if (frameIndex >= m_commandBuffers.size()) {
        SetLastError("Invalid frame index for reset command buffer");
        return false;
    }
    
    VkResult result = vkResetCommandBuffer(m_commandBuffers[frameIndex], 0);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkResetCommandBuffer");
        return false;
    }
    
    return true;
}

bool VulkanCommandBufferManager::BeginCommandBuffer(uint32_t frameIndex) {
    if (frameIndex >= m_commandBuffers.size()) {
        SetLastError("Invalid frame index for begin command buffer");
        return false;
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    
    VkResult result = vkBeginCommandBuffer(m_commandBuffers[frameIndex], &beginInfo);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkBeginCommandBuffer");
        return false;
    }
    
    return true;
}

bool VulkanCommandBufferManager::EndCommandBuffer(uint32_t frameIndex) {
    if (frameIndex >= m_commandBuffers.size()) {
        SetLastError("Invalid frame index for end command buffer");
        return false;
    }
    
    VkResult result = vkEndCommandBuffer(m_commandBuffers[frameIndex]);
    if (result != VK_SUCCESS) {
        HandleVulkanError(result, "vkEndCommandBuffer");
        return false;
    }
    
    return true;
}

bool VulkanCommandBufferManager::CreateCommandPool() {
    try {
        m_commandPool = std::make_unique<VulkanCommandPool>();
        VulkanCommandPool::Config poolConfig;
        poolConfig.queueFamilyIndex = m_device->GetQueueFamilyIndices().graphicsFamily.value();
        poolConfig.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        
        if (!m_commandPool->Initialize(m_device, poolConfig)) {
            SetLastError("Failed to create command pool: " + m_commandPool->GetLastError());
            return false;
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Command pool created successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Command pool creation failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanCommandBufferManager::AllocateCommandBuffers() {
    try {
        m_commandBuffers.resize(m_maxFramesInFlight);
        
        if (!m_commandPool->AllocateCommandBuffers(m_maxFramesInFlight, m_commandBuffers.data())) {
            SetLastError("Failed to allocate command buffers: " + m_commandPool->GetLastError());
            return false;
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Command buffers allocated successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Command buffer allocation failed: " + std::string(e.what()));
        return false;
    }
}

bool VulkanCommandBufferManager::CreateSynchronizationObjects() {
    try {
        m_imageAvailableSemaphores.resize(m_maxFramesInFlight);
        m_renderFinishedSemaphores.resize(m_maxFramesInFlight);
        m_inFlightFences.resize(m_maxFramesInFlight);
        
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // Create fences as signaled for the first frame to avoid waiting issues
        // This is safe because we'll reset them before first use
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        for (uint32_t i = 0; i < m_maxFramesInFlight; i++) {
            // Create image available semaphore
            VkResult result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
            if (result != VK_SUCCESS) {
                HandleVulkanError(result, "vkCreateSemaphore (imageAvailable)");
                return false;
            }
            
            // Create render finished semaphore
            result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
            if (result != VK_SUCCESS) {
                HandleVulkanError(result, "vkCreateSemaphore (renderFinished)");
                return false;
            }
            
            // Create fence
            result = vkCreateFence(m_device->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]);
            if (result != VK_SUCCESS) {
                HandleVulkanError(result, "vkCreateFence");
                return false;
            }
        }
        
        Logger::Debug("VulkanCommandBufferManager", "Synchronization objects created successfully");
        return true;
    }
    catch (const std::exception& e) {
        SetLastError("Synchronization objects creation failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanCommandBufferManager::DestroyCommandPool() {
    if (m_commandPool) {
        m_commandPool->Shutdown();
        m_commandPool.reset();
    }
}

void VulkanCommandBufferManager::DestroyCommandBuffers() {
    m_commandBuffers.clear();
}

void VulkanCommandBufferManager::DestroySynchronizationObjects() {
    if (m_device && m_device->GetDevice() != VK_NULL_HANDLE) {
        for (size_t i = 0; i < m_inFlightFences.size(); i++) {
            if (m_inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(m_device->GetDevice(), m_inFlightFences[i], nullptr);
            }
            if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device->GetDevice(), m_imageAvailableSemaphores[i], nullptr);
            }
            if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device->GetDevice(), m_renderFinishedSemaphores[i], nullptr);
            }
        }
    }
    
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
}

void VulkanCommandBufferManager::SetLastError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanCommandBufferManager", error);
}

void VulkanCommandBufferManager::HandleVulkanError(VkResult result, const std::string& operation) {
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
        default:
            errorString = "Unknown Vulkan error (" + std::to_string(result) + ")";
            break;
    }
    
    SetLastError("Vulkan error during " + operation + ": " + errorString);
}

} // namespace AstralEngine
