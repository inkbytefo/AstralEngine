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
    : m_graphicsDevice(nullptr)
    , m_isInitialized(false)
    , m_state(GpuResourceState::Unloaded) {
}

VulkanMesh::~VulkanMesh() {
    Shutdown();
}

bool VulkanMesh::Initialize(GraphicsDevice* graphicsDevice, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const AABB& boundingBox) {
    if (m_isInitialized) {
        SetError("Mesh already initialized");
        return false;
    }

    if (!graphicsDevice) {
        SetError("Invalid graphics device pointer");
        return false;
    }

    if (vertices.empty()) {
        SetError("Vertices vector is empty");
        return false;
    }

    m_graphicsDevice = graphicsDevice;
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

    // Staging buffer'ı temizle
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_graphicsDevice->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
    }

    m_vertices.clear();
    m_indices.clear();
    m_graphicsDevice = nullptr;
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
    return m_isInitialized;
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
    if (!m_vertexBuffer->Initialize(m_graphicsDevice->GetVulkanDevice(), bufferConfig)) {
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
    if (!m_indexBuffer->Initialize(m_graphicsDevice->GetVulkanDevice(), bufferConfig)) {
        SetError("Failed to initialize index buffer: " + m_indexBuffer->GetLastError());
        return false;
    }

    // Sadece buffer başlatma başarılı olduğunu bildir
    AE_DEBUG("VulkanMesh", "Index buffer initialized successfully, waiting for data upload.");
    return true;
}

bool VulkanMesh::UploadGpuData() {
    if (!m_graphicsDevice || !m_vertexBuffer || !m_indexBuffer) {
        SetError("Invalid graphics device or buffers for upload");
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

    if (vkCreateBuffer(m_graphicsDevice->GetDevice(), &stagingBufferInfo, nullptr, &m_stagingBuffer) != VK_SUCCESS) {
        SetError("Failed to create staging buffer");
        return false;
    }

    // Staging buffer için bellek ayır
    VkMemoryRequirements stagingMemRequirements;
    vkGetBufferMemoryRequirements(m_graphicsDevice->GetDevice(), m_stagingBuffer, &stagingMemRequirements);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemRequirements.size;
    stagingAllocInfo.memoryTypeIndex = m_graphicsDevice->GetVulkanDevice()->FindMemoryType(stagingMemRequirements.memoryTypeBits,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMemory;
    if (vkAllocateMemory(m_graphicsDevice->GetDevice(), &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        SetError("Failed to allocate staging buffer memory");
        vkDestroyBuffer(m_graphicsDevice->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(m_graphicsDevice->GetDevice(), m_stagingBuffer, stagingMemory, 0);

    // Staging buffer'ı haritala ve verileri kopyala
    void* stagingData;
    if (vkMapMemory(m_graphicsDevice->GetDevice(), stagingMemory, 0, totalSize, 0, &stagingData) != VK_SUCCESS) {
        SetError("Failed to map staging buffer memory");
        vkFreeMemory(m_graphicsDevice->GetDevice(), stagingMemory, nullptr);
        vkDestroyBuffer(m_graphicsDevice->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        return false;
    }

    // Vertex verilerini kopyala (başlangıçta)
    memcpy(stagingData, m_vertices.data(), vertexDataSize);
    
    // Index verilerini kopyala (vertex verilerinden sonra)
    memcpy(static_cast<char*>(stagingData) + vertexDataSize, m_indices.data(), indexDataSize);
    
    vkUnmapMemory(m_graphicsDevice->GetDevice(), stagingMemory);

    // Vertex buffer transferi için VulkanTransferManager'ı kullan
    m_graphicsDevice->GetTransferManager()->QueueTransfer(m_stagingBuffer, m_vertexBuffer->GetBuffer(), vertexDataSize);

    // Index buffer transferi için VulkanTransferManager'ı kullan
    m_graphicsDevice->GetTransferManager()->QueueTransfer(m_stagingBuffer, m_indexBuffer->GetBuffer(), indexDataSize);

    // Transferleri gönder
    m_graphicsDevice->GetTransferManager()->SubmitTransfers();

    // Staging buffer belleğini temizle (artık gerekli değil)
    vkFreeMemory(m_graphicsDevice->GetDevice(), stagingMemory, nullptr);

    // Mesh durumunu "Ready" olarak ayarla (transferler senkronize olarak tamamlandı)
    m_state = GpuResourceState::Ready;

    AE_DEBUG("VulkanMesh", "Mesh data uploaded successfully using VulkanTransferManager.");
    return true;
}

void VulkanMesh::SetError(const std::string& error) {
    m_lastError = error;
    AE_ERROR("VulkanMesh", "VulkanMesh error: %s", error.c_str());
}

} // namespace AstralEngine
