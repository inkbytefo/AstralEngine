#include "VulkanMesh.h"
#include "Core/Logger.h"

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
    if (!m_vertexBuffer->Initialize(m_graphicsDevice, bufferConfig)) {
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
    if (!m_indexBuffer->Initialize(m_graphicsDevice, bufferConfig)) {
        SetError("Failed to initialize index buffer: " + m_indexBuffer->GetLastError());
        return false;
    }

    // Sadece buffer başlatma başarılı olduğunu bildir
    AE_DEBUG("VulkanMesh", "Index buffer initialized successfully, waiting for data upload.");
    return true;
}

bool VulkanMesh::UploadGpuData() {
    if (!m_graphicsDevice || !m_vertexBuffer) {
        SetError("Invalid graphics device or vertex buffer for upload");
        return false;
    }

    // Set state to Uploading before enqueuing copies
    m_state = GpuResourceState::Uploading;
    AE_DEBUG("VulkanMesh", "Mesh upload started, state set to Uploading");

    // Vertex verilerini GPU'ya yükle
    VkDeviceSize vertexDataSize = sizeof(Vertex) * m_vertices.size();
    if (vertexDataSize > 0) {
        m_vertexBuffer->CopyDataFromHost(m_vertices.data(), vertexDataSize);
    }

    // Index verilerini GPU'ya yükle (eğer varsa)
    if (m_indexBuffer && !m_indices.empty()) {
        VkDeviceSize indexDataSize = sizeof(uint32_t) * m_indices.size();
        m_indexBuffer->CopyDataFromHost(m_indices.data(), indexDataSize);
    }

    // Register cleanup callback to set state to Ready after GPU completion
    m_graphicsDevice->GetTransferManager()->RegisterCleanupCallback([this]() {
        // Check for errors in vertex buffer
        if (m_vertexBuffer) {
            const std::string& vertexError = m_vertexBuffer->GetLastError();
            if (!vertexError.empty()) {
                AE_ERROR("VulkanMesh", "Vertex buffer error during upload: %s", vertexError.c_str());
                m_state = GpuResourceState::Failed;
                return;
            }
        }

        // Check for errors in index buffer if it exists
        if (m_indexBuffer) {
            const std::string& indexError = m_indexBuffer->GetLastError();
            if (!indexError.empty()) {
                AE_ERROR("VulkanMesh", "Index buffer error during upload: %s", indexError.c_str());
                m_state = GpuResourceState::Failed;
                return;
            }
        }

        // Set state to Ready only after GPU completion and error checking
        m_state = GpuResourceState::Ready;
        AE_DEBUG("VulkanMesh", "Mesh upload completed successfully, state set to Ready");
    });

    AE_DEBUG("VulkanMesh", "Mesh data upload queued successfully. State will be set to Ready after GPU completion.");
    return true;
}

void VulkanMesh::SetError(const std::string& error) {
    m_lastError = error;
    AE_ERROR("VulkanMesh", "VulkanMesh error: %s", error.c_str());
}

} // namespace AstralEngine
