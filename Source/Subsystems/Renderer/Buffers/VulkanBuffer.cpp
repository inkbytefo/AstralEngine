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
    , m_mapped(false) {
    
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


void VulkanBuffer::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanBuffer", "Error: {}", error);
}

} // namespace AstralEngine
