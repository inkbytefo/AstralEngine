#pragma once

#include "IRHIResource.h"
#include "IRHIPipeline.h"
#include "IRHIDescriptor.h"

namespace AstralEngine {

class IRHICommandList {
public:
    virtual ~IRHICommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void BeginRendering(const std::vector<IRHITexture*>& colorAttachments, IRHITexture* depthAttachment, const RHIRect2D& renderArea) = 0;
    virtual void BeginRendering(const std::vector<RHIRenderingAttachment>& colorAttachments, const RHIRenderingAttachment* depthAttachment, const RHIRect2D& renderArea) = 0;
    virtual void EndRendering() = 0;

    virtual void BindPipeline(IRHIPipeline* pipeline) = 0;
    
    virtual void SetViewport(const RHIViewport& viewport) = 0;
    virtual void SetScissor(const RHIRect2D& scissor) = 0;

    virtual void BindVertexBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset) = 0;
    virtual void BindIndexBuffer(IRHIBuffer* buffer, uint64_t offset, bool is32Bit) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;

    // Push constants, descriptors, etc.
    virtual void PushConstants(IRHIPipeline* pipeline, RHIShaderStage stage, uint32_t offset, uint32_t size, const void* data) = 0;
    virtual void BindDescriptorSet(IRHIPipeline* pipeline, IRHIDescriptorSet* descriptorSet, uint32_t setIndex) = 0;

    // Resource transitions (Internal/Utility)
    virtual void TransitionImageLayout(IRHITexture* texture, int oldLayout, int newLayout) = 0;
};

} // namespace AstralEngine
