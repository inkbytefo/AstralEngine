#pragma once

#include "IRHIResource.h"
#include "IRHIDescriptor.h"
#include <vector>

namespace AstralEngine {

struct RHIVertexInputAttribute {
    uint32_t location;
    uint32_t binding;
    RHIFormat format;
    uint32_t offset;
};

struct RHIVertexInputBinding {
    uint32_t binding;
    uint32_t stride;
    // InputRate: Vertex or Instance (simplifying for now)
    bool isInstanced;
};

struct RHIPushConstantRange {
    RHIShaderStage stageFlags;
    uint32_t offset;
    uint32_t size;
};

struct RHIPipelineStateDescriptor {
    IRHIShader* vertexShader = nullptr;
    IRHIShader* fragmentShader = nullptr;
    
    std::vector<RHIVertexInputBinding> vertexBindings;
    std::vector<RHIVertexInputAttribute> vertexAttributes;
    std::vector<RHIPushConstantRange> pushConstants;
    std::vector<IRHIDescriptorSetLayout*> descriptorSetLayouts;
    
    RHICullMode cullMode = RHICullMode::Back;
    RHIFrontFace frontFace = RHIFrontFace::CounterClockwise;
    
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    RHICompareOp depthCompareOp = RHICompareOp::Less;
    
    // Blending, etc. can be added here
};

class IRHIPipeline : public IRHIResource {
public:
    virtual ~IRHIPipeline() = default;
};

} // namespace AstralEngine
