#pragma once

#include <cstdint>
#include <string>

namespace AstralEngine {

enum class RHIFormat {
    Unknown,
    R8_UNORM,
    R8G8_UNORM,
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,
    R16G16_FLOAT,
    R16G16B16A16_FLOAT,
    R32_FLOAT,
    R32G32_FLOAT,
    R32G32B32_FLOAT,
    R32G32B32A32_FLOAT,
    D32_FLOAT,
    D24_UNORM_S8_UINT,
    D32_FLOAT_S8_UINT
};

enum class RHIShaderStage {
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
    Geometry = 1 << 3,
    TessControl = 1 << 4,
    TessEvaluation = 1 << 5,
    All = 0x7FFFFFFF
};

enum class RHIBufferUsage {
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5
};

enum class RHITextureUsage {
    TransferSrc = 1 << 0,
    TransferDst = 1 << 1,
    Sampled = 1 << 2,
    Storage = 1 << 3,
    ColorAttachment = 1 << 4,
    DepthStencilAttachment = 1 << 5
};

enum class RHIMemoryProperty {
    DeviceLocal = 1 << 0,
    HostVisible = 1 << 1,
    HostCoherent = 1 << 2
};

enum class RHICullMode {
    None,
    Front,
    Back,
    FrontAndBack
};

enum class RHIFrontFace {
    CounterClockwise,
    Clockwise
};

enum class RHICompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

struct RHIExtent2D {
    uint32_t width;
    uint32_t height;
};

struct RHIExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct RHIOffset3D {
    int32_t x;
    int32_t y;
    int32_t z;
};

enum class RHIFilter {
    Nearest,
    Linear
};

enum class RHISamplerAddressMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

struct RHISamplerDescriptor {
    RHIFilter minFilter = RHIFilter::Linear;
    RHIFilter magFilter = RHIFilter::Linear;
    RHISamplerAddressMode addressModeU = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode addressModeV = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode addressModeW = RHISamplerAddressMode::Repeat;
    bool anisotropyEnable = true;
    float maxAnisotropy = 16.0f;
};

inline RHIMemoryProperty operator|(RHIMemoryProperty a, RHIMemoryProperty b) {
    return static_cast<RHIMemoryProperty>(static_cast<int>(a) | static_cast<int>(b));
}

inline RHIMemoryProperty operator&(RHIMemoryProperty a, RHIMemoryProperty b) {
    return static_cast<RHIMemoryProperty>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool operator&&(RHIMemoryProperty a, RHIMemoryProperty b) {
    return static_cast<int>(a) & static_cast<int>(b);
}

inline RHIBufferUsage operator|(RHIBufferUsage a, RHIBufferUsage b) {
    return static_cast<RHIBufferUsage>(static_cast<int>(a) | static_cast<int>(b));
}

inline RHIBufferUsage operator&(RHIBufferUsage a, RHIBufferUsage b) {
    return static_cast<RHIBufferUsage>(static_cast<int>(a) & static_cast<int>(b));
}

inline RHITextureUsage operator|(RHITextureUsage a, RHITextureUsage b) {
    return static_cast<RHITextureUsage>(static_cast<int>(a) | static_cast<int>(b));
}

inline RHITextureUsage operator&(RHITextureUsage a, RHITextureUsage b) {
    return static_cast<RHITextureUsage>(static_cast<int>(a) & static_cast<int>(b));
}

inline RHIShaderStage operator|(RHIShaderStage a, RHIShaderStage b) {
    return static_cast<RHIShaderStage>(static_cast<int>(a) | static_cast<int>(b));
}

inline RHIShaderStage operator&(RHIShaderStage a, RHIShaderStage b) {
    return static_cast<RHIShaderStage>(static_cast<int>(a) & static_cast<int>(b));
}


enum class RHIDescriptorType {
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment
};

struct RHIDescriptorSetLayoutBinding {
    uint32_t binding;
    RHIDescriptorType descriptorType;
    uint32_t descriptorCount;
    RHIShaderStage stageFlags;
};

struct RHIViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};

struct RHIRect2D {
    RHIOffset3D offset;
    RHIExtent2D extent;
};

} // namespace AstralEngine
