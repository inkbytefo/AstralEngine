#include "Mesh.h"
#include "Core/Logger.h"

namespace AstralEngine {

    Mesh::Mesh(IRHIDevice* device, const ModelData& modelData)
        : m_device(device), m_vertexCount(0), m_indexCount(0), m_boundingBox(modelData.boundingBox) {

        if (modelData.vertices.empty()) {
            Logger::Warning("Mesh", "Attempted to create mesh with no vertices.");
            return;
        }

        m_vertexCount = static_cast<uint32_t>(modelData.vertices.size());
        m_indexCount = static_cast<uint32_t>(modelData.indices.size());

        // Create Vertex Buffer
        size_t vertexBufferSize = sizeof(Vertex) * m_vertexCount;
        m_vertexBuffer = m_device->CreateAndUploadBuffer(
            vertexBufferSize,
            RHIBufferUsage::Vertex,
            modelData.vertices.data()
        );

        // Create Index Buffer (if indices exist)
        if (m_indexCount > 0) {
            size_t indexBufferSize = sizeof(uint32_t) * m_indexCount;
            m_indexBuffer = m_device->CreateAndUploadBuffer(
                indexBufferSize,
                RHIBufferUsage::Index,
                modelData.indices.data()
            );
        }

        Logger::Info("Mesh", "Created mesh with {} vertices and {} indices.", m_vertexCount, m_indexCount);
    }

    Mesh::~Mesh() {
        // Buffers are shared_ptrs, will be released automatically
    }

    std::shared_ptr<Mesh> Mesh::CreateCube(IRHIDevice* device) {
        ModelData cubeData;
        cubeData.vertices = {
            // Back face
            { {-1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f, -1.0f} }, { { 1.0f, -1.0f, -1.0f} },
            { { 1.0f,  1.0f, -1.0f} }, { {-1.0f, -1.0f, -1.0f} }, { {-1.0f,  1.0f, -1.0f} },
            // Front face
            { {-1.0f, -1.0f,  1.0f} }, { { 1.0f, -1.0f,  1.0f} }, { { 1.0f,  1.0f,  1.0f} },
            { { 1.0f,  1.0f,  1.0f} }, { {-1.0f,  1.0f,  1.0f} }, { {-1.0f, -1.0f,  1.0f} },
            // Left face
            { {-1.0f,  1.0f,  1.0f} }, { {-1.0f,  1.0f, -1.0f} }, { {-1.0f, -1.0f, -1.0f} },
            { {-1.0f, -1.0f, -1.0f} }, { {-1.0f, -1.0f,  1.0f} }, { {-1.0f,  1.0f,  1.0f} },
            // Right face
            { { 1.0f,  1.0f,  1.0f} }, { { 1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f, -1.0f} },
            { { 1.0f, -1.0f, -1.0f} }, { { 1.0f,  1.0f,  1.0f} }, { { 1.0f, -1.0f,  1.0f} },
            // Bottom face
            { {-1.0f, -1.0f, -1.0f} }, { { 1.0f, -1.0f, -1.0f} }, { { 1.0f, -1.0f,  1.0f} },
            { { 1.0f, -1.0f,  1.0f} }, { {-1.0f, -1.0f,  1.0f} }, { {-1.0f, -1.0f, -1.0f} },
            // Top face
            { {-1.0f,  1.0f, -1.0f} }, { { 1.0f,  1.0f,  1.0f} }, { { 1.0f,  1.0f, -1.0f} },
            { { 1.0f,  1.0f,  1.0f} }, { {-1.0f,  1.0f, -1.0f} }, { {-1.0f,  1.0f,  1.0f} }
        };
        cubeData.isValid = true;
        return std::make_shared<Mesh>(device, cubeData);
    }

    std::shared_ptr<Mesh> Mesh::CreateQuad(IRHIDevice* device) {
        ModelData quadData;
        quadData.vertices = {
            { {-1.0f,  1.0f, 0.0f}, {0,0,1}, {0.0f, 1.0f} },
            { {-1.0f, -1.0f, 0.0f}, {0,0,1}, {0.0f, 0.0f} },
            { { 1.0f,  1.0f, 0.0f}, {0,0,1}, {1.0f, 1.0f} },
            { { 1.0f, -1.0f, 0.0f}, {0,0,1}, {1.0f, 0.0f} }
        };
        quadData.indices = { 0, 1, 2, 2, 1, 3 };
        quadData.isValid = true;
        return std::make_shared<Mesh>(device, quadData);
    }

    void Mesh::Bind(IRHICommandList* cmdList) {
        if (m_vertexBuffer) {
            cmdList->BindVertexBuffer(0, m_vertexBuffer.get(), 0);
        }
        if (m_indexBuffer) {
            cmdList->BindIndexBuffer(m_indexBuffer.get(), 0, true);
        }
    }

    void Mesh::Draw(IRHICommandList* cmdList) {
        if (!m_vertexBuffer) return;

        Bind(cmdList);

        if (m_indexBuffer) {
            cmdList->DrawIndexed(m_indexCount, 1, 0, 0, 0);
        } else {
            cmdList->Draw(m_vertexCount, 1, 0, 0);
        }
    }

}
