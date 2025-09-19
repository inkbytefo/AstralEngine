#include "VulkanMesh.h"
#include "../../../Core/Logger.h"

// Logger macros for convenience
#define AE_TRACE(cat, fmt, ...) AstralEngine::Logger::Trace(cat, fmt, ##__VA_ARGS__)
#define AE_DEBUG(cat, fmt, ...) AstralEngine::Logger::Debug(cat, fmt, ##__VA_ARGS__)
#define AE_INFO(cat, fmt, ...) AstralEngine::Logger::Info(cat, fmt, ##__VA_ARGS__)
#define AE_WARNING(cat, fmt, ...) AstralEngine::Logger::Warning(cat, fmt, ##__VA_ARGS__)
#define AE_ERROR(cat, fmt, ...) AstralEngine::Logger::Error(cat, fmt, ##__VA_ARGS__)
#define AE_CRITICAL(cat, fmt, ...) AstralEngine::Logger::Critical(cat, fmt, ##__VA_ARGS__)

namespace AstralEngine {

VulkanMesh::VulkanMesh()
    : m_device(nullptr)
    , m_isInitialized(false)
    , m_state(GpuResourceState::Unloaded)
    , m_uploadFence(VK_NULL_HANDLE) {
}

VulkanMesh::~VulkanMesh() {
    Shutdown();
}

bool VulkanMesh::Initialize(VulkanDevice* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const AABB& boundingBox) {
    if (m_isInitialized) {
        SetError("Mesh already initialized");
        return false;
    }

    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }

    if (vertices.empty()) {
        SetError("Vertices vector is empty");
        return false;
    }

    m_device = device;
    m_vertices = vertices;
    m_indices = indices;
    m_boundingBox = boundingBox;

    // Vertex buffer'ı oluştur
    if (!CreateVertexBuffer(vertices)) {
        AE_ERROR("VulkanMesh", "Failed to create vertex buffer: %s", m_lastError.c_str());
        Shutdown();
        return false;
    }

    // Index buffer'ı oluştur (eğer index varsa)
    if (!indices.empty()) {
        if (!CreateIndexBuffer(indices)) {
            AE_ERROR("VulkanMesh", "Failed to create index buffer: %s", m_lastError.c_str());
            Shutdown();
            return false;
        }
    }

    // Verileri GPU'ya yükle
    if (!UploadGpuData()) {
        AE_ERROR("VulkanMesh", "Failed to upload mesh data to GPU: %s", m_lastError.c_str());
        Shutdown();
        return false;
    }

    m_isInitialized = true;
    AE_INFO("VulkanMesh", "VulkanMesh initialized successfully with %zu vertices and %zu indices",
            vertices.size(), indices.size());
    return true;
}

void VulkanMesh::Shutdown() {
    if (m_vertexBuffer) {
        m_vertexBuffer->Shutdown();
        m_vertexBuffer.reset();
    }

    if (m_indexBuffer) {
        m_indexBuffer->Shutdown();
        m_indexBuffer.reset();
    }

    // Staging kaynaklarını temizle
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
    }

    if (m_stagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_stagingMemory, nullptr);
        m_stagingMemory = VK_NULL_HANDLE;
    }

    // Upload fence'i temizle
    if (m_uploadFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device->GetDevice(), m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
    }

    m_vertices.clear();
    m_indices.clear();
    m_device = nullptr;
    m_isInitialized = false;
    m_state = GpuResourceState::Unloaded;
    m_lastError.clear();
}

void VulkanMesh::Bind(VkCommandBuffer commandBuffer) const {
    if (!m_isInitialized) {
        AE_ERROR("VulkanMesh", "Cannot bind uninitialized mesh");
        return;
    }

    if (!commandBuffer) {
        AE_ERROR("VulkanMesh", "Invalid command buffer");
        return;
    }

    // Vertex buffer'ı bağla
    if (m_vertexBuffer && m_vertexBuffer->IsInitialized()) {
        VkBuffer vertexBuffers[] = { m_vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    } else {
        AE_ERROR("VulkanMesh", "Vertex buffer is not initialized");
        return;
    }

    // Index buffer'ı bağla (eğer varsa)
    if (m_indexBuffer && m_indexBuffer->IsInitialized()) {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

bool VulkanMesh::IsReady() const {
    if (!m_isInitialized) {
        return false;
    }
    
    // Upload fence durumunu kontrol et
    if (m_uploadFence != VK_NULL_HANDLE) {
        VkResult result = vkGetFenceStatus(m_device->GetDevice(), m_uploadFence);
        if (result == VK_SUCCESS) {
            m_uploadFence = VK_NULL_HANDLE;
            m_state = GpuResourceState::Ready;
            AE_DEBUG("VulkanMesh", "Mesh upload completed and fence cleaned up");
            return true;
        }
        return false;
    }
    
    // Fence zaten temizlendiyse ve state Ready ise
    return m_state == GpuResourceState::Ready;
}

bool VulkanMesh::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Vertex buffer için config oluştur
    VulkanBuffer::Config bufferConfig{};
    bufferConfig.size = bufferSize;
    bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferConfig.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Vertex buffer'ı oluştur
    m_vertexBuffer = std::make_unique<VulkanBuffer>();
    if (!m_vertexBuffer->Initialize(m_device, bufferConfig)) {
        SetError("Failed to initialize vertex buffer: " + m_vertexBuffer->GetLastError());
        return false;
    }

    // Sadece buffer başlatma başarılı olduğunu bildir
    AE_DEBUG("VulkanMesh", "Vertex buffer initialized successfully, waiting for data upload.");
    return true;
}

bool VulkanMesh::CreateIndexBuffer(const std::vector<uint32_t>& indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Index buffer için config oluştur
    VulkanBuffer::Config bufferConfig{};
    bufferConfig.size = bufferSize;
    bufferConfig.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferConfig.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Index buffer'ı oluştur
    m_indexBuffer = std::make_unique<VulkanBuffer>();
    if (!m_indexBuffer->Initialize(m_device, bufferConfig)) {
        SetError("Failed to initialize index buffer: " + m_indexBuffer->GetLastError());
        return false;
    }

    // Sadece buffer başlatma başarılı olduğunu bildir
    AE_DEBUG("VulkanMesh", "Index buffer initialized successfully, waiting for data upload.");
    return true;
}

bool VulkanMesh::UploadGpuData() {
    if (!m_device || !m_vertexBuffer || !m_indexBuffer) {
        SetError("Invalid device or buffers for upload");
        return false;
    }

    // Hem vertex hem de index verileri için toplam boyutu hesapla
    VkDeviceSize vertexDataSize = sizeof(Vertex) * m_vertices.size();
    VkDeviceSize indexDataSize = sizeof(uint32_t) * m_indices.size();
    VkDeviceSize totalSize = vertexDataSize + indexDataSize;

    if (totalSize == 0) {
        SetError("No data to upload");
        return false;
    }

    // Tek bir staging buffer oluştur
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = totalSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device->GetDevice(), &stagingBufferInfo, nullptr, &m_stagingBuffer) != VK_SUCCESS) {
        SetError("Failed to create staging buffer");
        return false;
    }

    // Staging buffer için bellek ayır
    VkMemoryRequirements stagingMemRequirements;
    vkGetBufferMemoryRequirements(m_device->GetDevice(), m_stagingBuffer, &stagingMemRequirements);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemRequirements.size;
    stagingAllocInfo.memoryTypeIndex = m_device->FindMemoryType(stagingMemRequirements.memoryTypeBits,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &stagingAllocInfo, nullptr, &m_stagingMemory) != VK_SUCCESS) {
        SetError("Failed to allocate staging buffer memory");
        vkDestroyBuffer(m_device->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(m_device->GetDevice(), m_stagingBuffer, m_stagingMemory, 0);

    // Staging buffer'ı haritala ve verileri kopyala
    void* stagingData;
    if (vkMapMemory(m_device->GetDevice(), m_stagingMemory, 0, totalSize, 0, &stagingData) != VK_SUCCESS) {
        SetError("Failed to map staging buffer memory");
        vkFreeMemory(m_device->GetDevice(), m_stagingMemory, nullptr);
        vkDestroyBuffer(m_device->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        m_stagingMemory = VK_NULL_HANDLE;
        return false;
    }

    // Vertex verilerini kopyala (başlangıçta)
    memcpy(stagingData, m_vertices.data(), vertexDataSize);
    
    // Index verilerini kopyala (vertex verilerinden sonra)
    memcpy(static_cast<char*>(stagingData) + vertexDataSize, m_indices.data(), indexDataSize);
    
    vkUnmapMemory(m_device->GetDevice(), m_stagingMemory);

    // SubmitToTransferQueue kullanarak verileri kopyala
    m_uploadFence = m_device->SubmitToTransferQueue([&](VkCommandBuffer commandBuffer) {
        // Vertex buffer kopyalama komutu
        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertexDataSize;
        vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_vertexBuffer->GetBuffer(), 1, &vertexCopyRegion);

        // Index buffer kopyalama komutu
        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = vertexDataSize;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indexDataSize;
        vkCmdCopyBuffer(commandBuffer, m_stagingBuffer, m_indexBuffer->GetBuffer(), 1, &indexCopyRegion);
    });

    if (m_uploadFence == VK_NULL_HANDLE) {
        SetError("Failed to submit transfer commands: " + m_device->GetLastError());
        return false;
    }

    // Mesh durumunu "Uploading" olarak ayarla
    m_state = GpuResourceState::Uploading;

    AE_DEBUG("VulkanMesh", "Mesh data upload submitted to transfer queue asynchronously.");
    return true;
}

void VulkanMesh::SetError(const std::string& error) {
    m_lastError = error;
    AE_ERROR("VulkanMesh", "VulkanMesh error: %s", error.c_str());
}

} // namespace AstralEngine
