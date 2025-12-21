#pragma once

#include "RHI_Types.h"

namespace AstralEngine {

class IRHIResource {
public:
    virtual ~IRHIResource() = default;
};

class IRHIBuffer : public IRHIResource {
public:
    virtual ~IRHIBuffer() = default;
    virtual uint64_t GetSize() const = 0;
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
};

class IRHITexture : public IRHIResource {
public:
    virtual ~IRHITexture() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual RHIFormat GetFormat() const = 0;
};

class IRHIShader : public IRHIResource {
public:
    virtual ~IRHIShader() = default;
    virtual RHIShaderStage GetStage() const = 0;
};

} // namespace AstralEngine
