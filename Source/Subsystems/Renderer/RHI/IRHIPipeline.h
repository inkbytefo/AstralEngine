#pragma once

#include "IRHIResource.h"
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

struct RHIPipelineStateDescriptor {
    IRHIShader* vertexShader = nullptr;
    IRHIShader* fragmentShader = nullptr;
    
    std::vector<RHIVertexInputBinding> vertexBindings;
    std::vector<RHIVertexInputAttribute> vertexAttributes;
    
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
