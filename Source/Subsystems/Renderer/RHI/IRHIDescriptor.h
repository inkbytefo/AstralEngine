#pragma once

#include "IRHIResource.h"
#include <vector>

namespace AstralEngine {

class IRHIDescriptorSetLayout : public IRHIResource {
public:
    virtual ~IRHIDescriptorSetLayout() = default;
};

class IRHIDescriptorSet : public IRHIResource {
public:
    virtual ~IRHIDescriptorSet() = default;

    virtual void UpdateUniformBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset, uint64_t range) = 0;
    virtual void UpdateCombinedImageSampler(uint32_t binding, IRHITexture* texture, IRHISampler* sampler) = 0;
};

} // namespace AstralEngine
