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
    , m_isInitialized(false) {
}

VulkanMesh::~VulkanMesh() {
    Shutdown();
}

bool VulkanMesh::Initialize(VulkanDevice* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
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

    m_vertices.clear();
    m_indices.clear();
    m_device = nullptr;
    m_isInitialized = false;
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

    // Veriyi buffer'a kopyala
    if (!CopyDataToBuffer(m_vertexBuffer.get(), bufferSize, vertices.data())) {
        return false;
    }

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

    // Veriyi buffer'a kopyala
    if (!CopyDataToBuffer(m_indexBuffer.get(), bufferSize, indices.data())) {
        return false;
    }

    return true;
}

bool VulkanMesh::CopyDataToBuffer(VulkanBuffer* dstBuffer, VkDeviceSize bufferSize, const void* data) {
    if (!dstBuffer || !data || bufferSize == 0) {
        SetError("Invalid parameters for data copy");
        return false;
    }

    // 1. Geçici bir staging buffer oluştur (CPU'dan erişilebilir)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           stagingBuffer, stagingBufferMemory);

    // 2. Staging buffer'ı map et ve veriyi kopyala
    void* mappedData;
    vkMapMemory(m_device->GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_device->GetDevice(), stagingBufferMemory);

    // 3. Kopyalama komutunu GPU'ya gönder
    m_device->SubmitSingleTimeCommands([&](VkCommandBuffer commandBuffer) {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer->GetBuffer(), 1, &copyRegion);
    });

    // 4. Geçici staging buffer'ı temizle
    vkDestroyBuffer(m_device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->GetDevice(), stagingBufferMemory, nullptr);

    AE_DEBUG("VulkanMesh", "Data successfully copied to device-local buffer via staging.");
    return true;
}

void VulkanMesh::SetError(const std::string& error) {
    m_lastError = error;
    AE_ERROR("VulkanMesh", "VulkanMesh error: %s", error.c_str());
}

} // namespace AstralEngine
