#pragma once

#include "RHI_Types.h"
#include "IRHIResource.h"
#include "IRHIPipeline.h"
#include "IRHICommandList.h"
#include "IRHIDescriptor.h"
#include <memory>
#include <vector>
#include <span>

namespace AstralEngine {

class Window; // Forward declaration

class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Resource Creation
    virtual std::shared_ptr<IRHIBuffer> CreateBuffer(uint64_t size, RHIBufferUsage usage, RHIMemoryProperty memoryProperties) = 0;
    
    // Creates a DeviceLocal buffer and uploads data using a staging buffer
    virtual std::shared_ptr<IRHIBuffer> CreateAndUploadBuffer(uint64_t size, RHIBufferUsage usage, const void* data) = 0;

    virtual std::shared_ptr<IRHITexture> CreateTexture2D(uint32_t width, uint32_t height, RHIFormat format, RHITextureUsage usage) = 0;
    virtual std::shared_ptr<IRHITexture> CreateAndUploadTexture(uint32_t width, uint32_t height, RHIFormat format, const void* data) = 0;
    virtual std::shared_ptr<IRHISampler> CreateSampler(const RHISamplerDescriptor& descriptor) = 0;

    virtual std::shared_ptr<IRHIShader> CreateShader(RHIShaderStage stage, std::span<const uint8_t> code) = 0;
    virtual std::shared_ptr<IRHIPipeline> CreateGraphicsPipeline(const RHIPipelineStateDescriptor& descriptor) = 0;

    // Descriptors
    virtual std::shared_ptr<IRHIDescriptorSetLayout> CreateDescriptorSetLayout(const std::vector<RHIDescriptorSetLayoutBinding>& bindings) = 0;
    virtual std::shared_ptr<IRHIDescriptorSet> AllocateDescriptorSet(IRHIDescriptorSetLayout* layout) = 0;

    // Command List
    virtual std::shared_ptr<IRHICommandList> CreateCommandList() = 0;
    virtual void SubmitCommandList(IRHICommandList* commandList) = 0;

    // Frame management
    virtual void BeginFrame() = 0;
    virtual void Present() = 0;
    
    // Swapchain interaction
    virtual IRHITexture* GetCurrentBackBuffer() = 0;
    virtual IRHITexture* GetDepthBuffer() = 0;
    virtual uint32_t GetCurrentFrameIndex() const = 0;
    
    // Waiting
    virtual void WaitIdle() = 0;
};

// Factory function
std::shared_ptr<IRHIDevice> CreateVulkanDevice(Window* window);

} // namespace AstralEngine
