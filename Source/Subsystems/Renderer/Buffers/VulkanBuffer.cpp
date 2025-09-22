#include "VulkanBuffer.h"
#include "Core/Logger.h"
#include "../GraphicsDevice.h"
#include "../Core/VulkanTransferManager.h"

namespace AstralEngine {

VulkanBuffer::VulkanBuffer() 
    : m_graphicsDevice(nullptr)
    , m_device(nullptr)
    , m_buffer(VK_NULL_HANDLE)
    , m_bufferMemory(VK_NULL_HANDLE)
    , m_size(0)
    , m_usage(0)
    , m_properties(0)
    , m_mappedData(nullptr)
    , m_isInitialized(false)
    , m_mapped(false) {
    AstralEngine::Logger::Trace("VulkanBuffer", "VulkanBuffer created");
}

VulkanBuffer::~VulkanBuffer() {
    if (m_isInitialized) {
        Shutdown();
    }
    AstralEngine::Logger::Trace("VulkanBuffer", "VulkanBuffer destroyed");
}

bool VulkanBuffer::Initialize(GraphicsDevice* graphicsDevice, const Config& config) {
    if (m_isInitialized) {
        AstralEngine::Logger::Warning("VulkanBuffer", "VulkanBuffer already initialized");
        return true;
    }
    
    if (!graphicsDevice) {
        SetError("Invalid graphics device pointer");
        return false;
    }

    m_graphicsDevice = graphicsDevice;
    m_device = graphicsDevice->GetVulkanDevice();

    if (!m_device) {
        SetError("VulkanDevice not available from GraphicsDevice");
        return false;
    }
    
    if (config.size == 0) {
        SetError("Buffer size cannot be zero");
        return false;
    }
    
    m_size = config.size;
    m_usage = config.usage;
    m_properties = config.properties;
    
    AstralEngine::Logger::Info("VulkanBuffer", "Initializing buffer: size={0} bytes, usage={1}, properties={2}",
                 m_size, static_cast<uint32_t>(m_usage), static_cast<uint32_t>(m_properties));
    
    if (!m_device->CreateBuffer(m_size, m_usage, m_properties, m_buffer, m_bufferMemory)) {
        SetError("Failed to create buffer: " + m_device->GetLastError());
        return false;
    }
        
    m_isInitialized = true;
    AstralEngine::Logger::Info("VulkanBuffer", "Buffer initialized successfully");
    return true;
}

void VulkanBuffer::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    AstralEngine::Logger::Info("VulkanBuffer", "Shutting down buffer...");

    if (m_mapped) {
        Unmap();
    }

    // Check if we have valid Vulkan handles to destroy
    if (m_buffer != VK_NULL_HANDLE && m_bufferMemory != VK_NULL_HANDLE) {
        // First, try to use GraphicsDevice's safe deletion queue if available and initialized
        if (m_graphicsDevice != nullptr && m_graphicsDevice->IsInitialized()) {
            // GraphicsDevice is available and initialized, use safe deletion queue
            m_graphicsDevice->QueueBufferForDeletion(m_buffer, m_bufferMemory);
            AstralEngine::Logger::Debug("VulkanBuffer", "Queued buffer for deletion via GraphicsDevice.");
        } else {
            // GraphicsDevice is not available or not initialized, use direct destruction
            AstralEngine::Logger::Warning("VulkanBuffer", "GraphicsDevice not available or not initialized, using direct destruction");

            // Validate m_device before using it for direct destruction
            if (m_device != nullptr && m_device->GetDevice() != VK_NULL_HANDLE) {
                // Destroy buffer and free memory directly
                vkDestroyBuffer(m_device->GetDevice(), m_buffer, nullptr);
                vkFreeMemory(m_device->GetDevice(), m_bufferMemory, nullptr);
                AstralEngine::Logger::Debug("VulkanBuffer", "Buffer destroyed directly using VulkanDevice.");
            } else {
                // m_device is invalid - this is a critical error but we can't do much about it
                AstralEngine::Logger::Error("VulkanBuffer", "Cannot destroy buffer: both GraphicsDevice and VulkanDevice are invalid. Memory leak likely.");
                AstralEngine::Logger::Error("VulkanBuffer", "Buffer handle: {0}, Memory handle: {1}", reinterpret_cast<void*>(m_buffer), reinterpret_cast<void*>(m_bufferMemory));
            }
        }
    }
    // Set all member pointers to null to prevent UAF
    m_buffer = VK_NULL_HANDLE;
    m_bufferMemory = VK_NULL_HANDLE;
    m_graphicsDevice = nullptr;
    m_device = nullptr;
    m_mappedData = nullptr;
    m_isInitialized = false;

    AstralEngine::Logger::Info("VulkanBuffer", "Buffer shutdown completed.");
}

void* VulkanBuffer::Map() {
    if (!m_isInitialized) {
        SetError("Buffer not initialized");
        return nullptr;
    }
    
    if (m_mapped) {
        AstralEngine::Logger::Warning("VulkanBuffer", "Buffer already mapped");
        return m_mappedData;
    }
    
    VkResult result = vkMapMemory(m_device->GetDevice(), m_bufferMemory, 0, m_size, 0, &m_mappedData);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to map buffer memory, VkResult: " + std::to_string(result));
        return nullptr;
    }
    
    m_mapped = true;
    AstralEngine::Logger::Debug("VulkanBuffer", "Buffer mapped successfully");
    return m_mappedData;
}

void VulkanBuffer::Unmap() {
    if (!m_isInitialized || !m_mapped) {
        return;
    }
    
    vkUnmapMemory(m_device->GetDevice(), m_bufferMemory);
    m_mappedData = nullptr;
    m_mapped = false;
    
    AstralEngine::Logger::Debug("VulkanBuffer", "Buffer unmapped successfully");
}

void VulkanBuffer::SetError(const std::string& error) {
    m_lastError = error;
    AstralEngine::Logger::Error("VulkanBuffer", "VulkanBuffer Error: {0}", error);
}

void VulkanBuffer::CopyDataFromHost(const void* data, VkDeviceSize dataSize) {
    if (!m_isInitialized) {
        SetError("Buffer not initialized");
        return;
    }

    if (!data || dataSize == 0) {
        SetError("Invalid data or data size for host copy");
        return;
    }

    if (dataSize > m_size) {
        SetError("Data size exceeds buffer size");
        return;
    }

    // Validate that buffer has TRANSFER_DST usage flag for copy operations
    if (!(m_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        SetError("Buffer does not have VK_BUFFER_USAGE_TRANSFER_DST_BIT flag required for copy operations");
        AstralEngine::Logger::Error("VulkanBuffer", "VulkanBuffer::CopyDataFromHost failed - Buffer usage flags: {0}, missing TRANSFER_DST_BIT",
                      static_cast<uint32_t>(m_usage));
        return;
    }

    m_state = GpuResourceState::Uploading;
    AstralEngine::Logger::Info("VulkanBuffer", "Starting data copy from host: size={0} bytes", dataSize);

    // Create a temporary staging buffer using the transfer manager
    VulkanTransferManager* transferManager = m_graphicsDevice->GetTransferManager();
    VkBuffer destinationBuffer = m_buffer;

    // Create temporary staging buffer BEFORE queuing the lambda
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    if (!m_device->CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              stagingBuffer, stagingMemory)) {
        SetError("Failed to create temporary staging buffer: " + m_device->GetLastError());
        m_state = GpuResourceState::Failed;
        AstralEngine::Logger::Error("VulkanBuffer", "Buffer state set to Failed due to staging buffer creation failure.");
        return;
    }

    // Map and copy data to staging buffer BEFORE queuing the lambda
    void* mappedData;
    if (vkMapMemory(m_device->GetDevice(), stagingMemory, 0, dataSize, 0, &mappedData) != VK_SUCCESS) {
        AstralEngine::Logger::Error("VulkanBuffer", "Failed to map staging buffer memory");
        vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_device->GetDevice(), stagingMemory, nullptr);
        SetError("Failed to map staging buffer memory");
        m_state = GpuResourceState::Failed;
        AstralEngine::Logger::Error("VulkanBuffer", "Buffer state set to Failed due to staging buffer mapping failure.");
        return;
    }

    // Copy data to staging buffer immediately (no pointer capture in lambda)
    memcpy(mappedData, data, static_cast<size_t>(dataSize));
    vkUnmapMemory(m_device->GetDevice(), stagingMemory);

    // Queue the transfer operation with only buffer handles and sizes (no raw pointers)
    transferManager->QueueTransfer([=](VkCommandBuffer cmd) {
        // Record the copy command - only Vulkan API calls, no raw pointer dependencies
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dataSize;
        vkCmdCopyBuffer(cmd, stagingBuffer, destinationBuffer, 1, &copyRegion);
    });

    // Register cleanup callback to be executed after GPU transfer completion
    transferManager->RegisterCleanupCallback([=]() {
        // This callback will be executed after the transfer fence signals,
        // ensuring the GPU has completed the transfer operation
        m_graphicsDevice->QueueBufferForDeletion(stagingBuffer, stagingMemory);
        AstralEngine::Logger::Debug("VulkanBuffer", "Staging buffer cleanup executed after GPU transfer completion.");

        // Set buffer state to Ready only after GPU transfer completion
        m_state = GpuResourceState::Ready;
        AstralEngine::Logger::Debug("VulkanBuffer", "Buffer state set to Ready after GPU transfer completion.");
    });

    // Note: Transfer submission is now handled by GraphicsDevice::EndFrame()
    // This allows for proper batching of multiple transfers for better performance

    AstralEngine::Logger::Info("VulkanBuffer", "Data copy to buffer queued successfully. State will be set to Ready after GPU completion.");
}

} // namespace AstralEngine