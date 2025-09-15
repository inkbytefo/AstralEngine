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
        // Buffer oluştur
        if (!CreateBuffer()) {
            return false;
        }
        
        Logger::Debug("VulkanBuffer", "Buffer created successfully");
        
        // Bellek ayır
        if (!AllocateMemory()) {
            return false;
        }
        
        Logger::Debug("VulkanBuffer", "Memory allocated successfully");
        
        // Belleği buffer'a bağla
        BindMemory();
        
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

bool VulkanBuffer::CreateBuffer() {
    Logger::Debug("VulkanBuffer", "Creating buffer");
    
    // Buffer create info
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_size;
    bufferInfo.usage = m_usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Tek queue family kullanıyoruz
    
    // Buffer oluştur
    VkResult result = vkCreateBuffer(m_device->GetDevice(), &bufferInfo, nullptr, &m_buffer);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to create buffer, VkResult: " + std::to_string(result));
        Logger::Error("VulkanBuffer", "Failed to create buffer: {}", static_cast<int32_t>(result));
        return false;
    }
    
    Logger::Debug("VulkanBuffer", "Buffer created with handle: {}", 
                 reinterpret_cast<uintptr_t>(m_buffer));
    return true;
}

bool VulkanBuffer::AllocateMemory() {
    Logger::Debug("VulkanBuffer", "Allocating buffer memory");
    
    // Bellek gereksinimlerini al
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device->GetDevice(), m_buffer, &memRequirements);
    
    Logger::Debug("VulkanBuffer", "Memory requirements: size={}, alignment={}, typeBits={}", 
                 memRequirements.size, memRequirements.alignment, memRequirements.memoryTypeBits);
    
    // Bellek tipini bul
    uint32_t memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, m_properties);
    
    if (memoryTypeIndex == UINT32_MAX) {
        SetError("Failed to find suitable memory type");
        Logger::Error("VulkanBuffer", "Failed to find suitable memory type");
        return false;
    }
    
    // Bellek ayırma info
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    // Bellek ayır
    VkResult result = vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_bufferMemory);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to allocate buffer memory, VkResult: " + std::to_string(result));
        Logger::Error("VulkanBuffer", "Failed to allocate buffer memory: {}", static_cast<int32_t>(result));
        return false;
    }
    
    Logger::Debug("VulkanBuffer", "Memory allocated with handle: {}", 
                 reinterpret_cast<uintptr_t>(m_bufferMemory));
    return true;
}

void VulkanBuffer::BindMemory() {
    Logger::Debug("VulkanBuffer", "Binding memory to buffer");
    
    // Belleği buffer'a bağla
    VkResult result = vkBindBufferMemory(m_device->GetDevice(), m_buffer, m_bufferMemory, 0);
    
    if (result != VK_SUCCESS) {
        SetError("Failed to bind buffer memory, VkResult: " + std::to_string(result));
        Logger::Error("VulkanBuffer", "Failed to bind buffer memory: {}", static_cast<int32_t>(result));
        throw std::runtime_error("Failed to bind buffer memory");
    }
    
    Logger::Debug("VulkanBuffer", "Memory bound to buffer successfully");
}

uint32_t VulkanBuffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    // Fiziksel cihazın bellek özelliklerini al
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device->GetPhysicalDevice(), &memProperties);
    
    // Uygun bellek tipini ara
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Bellek tipi filter'da var mı ve istenen özelliklere sahip mi?
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            Logger::Debug("VulkanBuffer", "Found suitable memory type: {}", i);
            return i;
        }
    }
    
    Logger::Error("VulkanBuffer", "Failed to find suitable memory type");
    return UINT32_MAX;
}

void VulkanBuffer::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanBuffer", "Error: {}", error);
}

} // namespace AstralEngine
