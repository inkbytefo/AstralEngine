#pragma once

#include "../RHI/IRHIResource.h"
#include "../RHI/IRHIDevice.h"
#include "../RHI/IRHICommandList.h"
#include "../../Asset/AssetData.h"

#include <memory>

namespace AstralEngine {

    class Mesh {
    public:
        Mesh(IRHIDevice* device, const ModelData& modelData);
        ~Mesh();

        void Bind(IRHICommandList* cmdList);
        void Draw(IRHICommandList* cmdList);

        uint32_t GetVertexCount() const { return m_vertexCount; }
        uint32_t GetIndexCount() const { return m_indexCount; }

    private:
        IRHIDevice* m_device;
        std::shared_ptr<IRHIBuffer> m_vertexBuffer;
        std::shared_ptr<IRHIBuffer> m_indexBuffer;
        uint32_t m_vertexCount;
        uint32_t m_indexCount;
    };

}
