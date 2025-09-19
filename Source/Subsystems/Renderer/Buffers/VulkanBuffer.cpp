#include "VulkanBuffer.h"
#include "../../../Core/Logger.h"

namespace AstralEngine {

VulkanBuffer::VulkanBuffer() 
    : m_device(nullptr)
    , m_buffer(VK_NULL_HANDLE)
    , m_bufferMemory(VK_NULL_HANDLE)
    , m_size(0)
    , m_usage(0)
    , m_properties(0)
    , m_mappedData(nullptr)
    , m_isInitialized(false)
    , m_mapped(false)
    , m_autoCleanupStaging(true) {
    
    Logger::Debug("VulkanBuffer", "VulkanBuffer created");
}

VulkanBuffer::~VulkanBuffer() {
    if (m_isInitialized) {
        Shutdown();
    }
    Logger::Debug("VulkanBuffer", "VulkanBuffer destroyed");
}

bool VulkanBuffer::Initialize(VulkanDevice* device, const Config& config) {
    if (m_isInitialized) {
        Logger::Warning("VulkanBuffer", "VulkanBuffer already initialized");
        return true;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    if (config.size == 0) {
        SetError("Buffer size cannot be zero");
        return false;
    }
    
    m_device = device;
    m_size = config.size;
    m_usage = config.usage;
    m_properties = config.properties;
    
    Logger::Info("VulkanBuffer", "Initializing buffer: size={} bytes, usage={}, properties={}", 
                m_size, static_cast<uint32_t>(m_usage), static_cast<uint32_t>(m_properties));
    
    try {
        // Buffer ve belleği cihaz üzerinden oluştur
        if (!m_device->CreateBuffer(m_size, m_usage, m_properties, m_buffer, m_bufferMemory)) {
            SetError("Failed to create buffer through device: " + m_device->GetLastError());
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanBuffer", "Buffer initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during buffer initialization: ") + e.what());
        Logger::Error("VulkanBuffer", "Buffer initialization failed: {}", e.what());
        return false;
    }
}

void VulkanBuffer::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    Logger::Info("VulkanBuffer", "Shutting down buffer");
    
    // Eğer mapped ise unmap yap
    if (m_mapped) {
        Unmap();
    }
    
    // Belleği serbest bırak
    if (m_bufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_bufferMemory, nullptr);
        m_bufferMemory = VK_NULL_HANDLE;
        Logger::Debug("VulkanBuffer", "Buffer memory freed");
    }
    
    // Buffer'ı yok et
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->GetDevice(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        Logger::Debug("VulkanBuffer", "Buffer destroyed");
    }
    
    m_device = nullptr;
    m_size = 0;
    m_usage = 0;
    m_properties = 0;
    m_mappedData = nullptr;
    m_lastError.clear();
    m_isInitialized = false;
    
    Logger::Info("VulkanBuffer", "Buffer shutdown completed");
}

void* VulkanBuffer::Map() {
    if (!m_isInitialized) {
        SetError("Buffer not initialized");
        return nullptr;
    }
    
    if (m_mapped) {
        Logger::Warning("VulkanBuffer", "Buffer already mapped");
        return m_mappedData;
    }
    
    // Belleği map et
    VkResult result = vkMapMemory(m_device->GetDevice(), m_bufferMemory, 0, m_size, 0, &m_mappedData);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to map buffer memory, VkResult: " + std::to_string(result));
        Logger::Error("VulkanBuffer", "Failed to map buffer memory: {}", static_cast<int32_t>(result));
        return nullptr;
    }
    
    m_mapped = true;
    Logger::Debug("VulkanBuffer", "Buffer mapped successfully");
    return m_mappedData;
}

void VulkanBuffer::Unmap() {
    if (!m_isInitialized || !m_mapped) {
        return;
    }
    
    vkUnmapMemory(m_device->GetDevice(), m_bufferMemory);
    m_mappedData = nullptr;
    m_mapped = false;
    
    Logger::Debug("VulkanBuffer", "Buffer unmapped successfully");
}

void VulkanBuffer::Release() {
    m_buffer = VK_NULL_HANDLE;
    m_bufferMemory = VK_NULL_HANDLE;
    m_isInitialized = false; // Prevent Shutdown from running
}


void VulkanBuffer::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanBuffer", "Error: {}", error);
}

VkFence VulkanBuffer::CopyDataFromHost(const void* data, VkDeviceSize dataSize, bool async) {
    if (!m_isInitialized) {
        SetError("Buffer not initialized");
        return VK_NULL_HANDLE;
    }
    
    if (!data || dataSize == 0) {
        SetError("Invalid data or data size");
        return VK_NULL_HANDLE;
    }
    
    if (dataSize > m_size) {
        SetError("Data size exceeds buffer size");
        return VK_NULL_HANDLE;
    }
    
    // Önceki staging kaynaklarını temizle
    CleanupStagingResources();
    
    Logger::Info("VulkanBuffer", "Starting data copy from host: size={} bytes, async={}", dataSize, async);
    
    try {
        // Staging buffer oluştur (CPU erişilebilir, transfer için)
        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = dataSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        if (!m_device->CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingBuffer, stagingMemory)) {
            SetError("Failed to create staging buffer: " + m_device->GetLastError());
            m_state = GpuResourceState::Failed;
            return VK_NULL_HANDLE;
        }
        
        // Staging buffer'ı map et ve veriyi kopyala
        void* mappedData;
        VkResult result = vkMapMemory(m_device->GetDevice(), stagingMemory, 0, dataSize, 0, &mappedData);
        if (result != VK_SUCCESS) {
            SetError("Failed to map staging buffer memory, VkResult: " + std::to_string(result));
            vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
            vkFreeMemory(m_device->GetDevice(), stagingMemory, nullptr);
            m_state = GpuResourceState::Failed;
            return VK_NULL_HANDLE;
        }
        
        memcpy(mappedData, data, static_cast<size_t>(dataSize));
        vkUnmapMemory(m_device->GetDevice(), stagingMemory);
        
        // Fence oluştur (asenkron için)
        VkFence fence = VK_NULL_HANDLE;
        if (async) {
            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            
            result = vkCreateFence(m_device->GetDevice(), &fenceInfo, nullptr, &fence);
            if (result != VK_SUCCESS) {
                SetError("Failed to create fence, VkResult: " + std::to_string(result));
                vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
                vkFreeMemory(m_device->GetDevice(), stagingMemory, nullptr);
                m_state = GpuResourceState::Failed;
                return VK_NULL_HANDLE;
            }
        }
        
        // Transfer komutunu başlat
        VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands();
        if (!commandBuffer) {
            SetError("Failed to begin single time commands");
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device->GetDevice(), fence, nullptr);
            }
            vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
            vkFreeMemory(m_device->GetDevice(), stagingMemory, nullptr);
            m_state = GpuResourceState::Failed;
            return VK_NULL_HANDLE;
        }
        
        // Buffer kopyalama işlemi
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dataSize;
        
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_buffer, 1, &copyRegion);
        
        // Komut buffer'ını bitir
        if (async) {
            m_device->EndSingleTimeCommands(commandBuffer, fence);
            m_state = GpuResourceState::Uploading;
            
            // Staging kaynaklarını sakla (daha sonra temizlemek için)
            m_stagingBuffer = stagingBuffer;
            m_stagingMemory = stagingMemory;
            m_uploadFence = fence;
            
            Logger::Info("VulkanBuffer", "Async data copy started, fence created");
            return fence;
        } else {
            m_device->EndSingleTimeCommands(commandBuffer);
            
            // Staging buffer'ı hemen temizle
            vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
            vkFreeMemory(m_device->GetDevice(), stagingMemory, nullptr);
            
            m_state = GpuResourceState::Ready;
            Logger::Info("VulkanBuffer", "Sync data copy completed");
            return VK_NULL_HANDLE;
        }
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during data copy: ") + e.what());
        Logger::Error("VulkanBuffer", "Data copy failed: {}", e.what());
        m_state = GpuResourceState::Failed;
        return VK_NULL_HANDLE;
    }
}

void VulkanBuffer::CleanupStagingResources() {
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        Logger::Debug("VulkanBuffer", "Staging buffer destroyed");
    }
    
    if (m_stagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_stagingMemory, nullptr);
        m_stagingMemory = VK_NULL_HANDLE;
        Logger::Debug("VulkanBuffer", "Staging memory freed");
    }
    
    if (m_uploadFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device->GetDevice(), m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
        Logger::Debug("VulkanBuffer", "Upload fence destroyed");
    }
}

bool VulkanBuffer::IsUploadComplete() const {
    if (m_uploadFence == VK_NULL_HANDLE) {
        return m_state == GpuResourceState::Ready;
    }
    
    VkResult result = vkGetFenceStatus(m_device->GetDevice(), m_uploadFence);
    if (result == VK_SUCCESS) {
        // Upload tamamlandı, durumu güncelle
        m_state = GpuResourceState::Ready;
        Logger::Debug("VulkanBuffer", "Upload completed, state updated to Ready");
        return true;
    } else if (result == VK_NOT_READY) {
        // Henüz tamamlanmadı
        return false;
    } else {
        // Hata oluştu
        Logger::Error("VulkanBuffer", "Fence status check failed: {}", static_cast<int32_t>(result));
        return false;
    }
}

} // namespace AstralEngine
