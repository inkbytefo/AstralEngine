#pragma once

#include <astral/core/handle.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace astral::gpu {

// --- Handles ---------------------------------------------------------------------------
struct BufferTag;
struct TextureTag;
struct PipelineTag;
using BufferHandle = Handle<BufferTag>;
using TextureHandle = Handle<TextureTag>;
using PipelineHandle = Handle<PipelineTag>;
struct AccelerationStructureTag;
/// A ray tracing acceleration structure (bottom level: geometry, top level: instances).
using AccelerationStructureHandle = Handle<AccelerationStructureTag>;
struct QueryPoolTag;
/// A pool of GPU timestamp queries (profiling).
using QueryPoolHandle = Handle<QueryPoolTag>;

/// Monotonic submission id. `Device::wait(ticket)` blocks until that submission finished.
struct SubmitTicket {
    std::uint64_t value = 0;
};

// --- Flags helper ----------------------------------------------------------------------
template <class E>
    requires std::is_enum_v<E>
constexpr E operator|(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}
template <class E>
    requires std::is_enum_v<E>
constexpr bool has_flag(E value, E flag) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
}

// --- Enums -----------------------------------------------------------------------------
enum class Format : std::uint8_t {
    Undefined,
    RGBA8Unorm,
    RGBA8Srgb,
    BGRA8Unorm,
    BGRA8Srgb,
    RG16Float,
    RGBA16Float,
    R32Float,
    RGBA32Float,
    R11G11B10Float,
    D32Float,
    BC5Unorm,  ///< Two channels (normal map XY), 4x4 blocks of 16 bytes. Needs Feature::TextureCompressionBC.
    BC7Unorm,  ///< RGBA, 4x4 blocks of 16 bytes.
    BC7Srgb,
};

std::uint32_t format_size(Format format) noexcept; ///< Bytes per texel (0 for block-compressed formats).
bool is_compressed(Format format) noexcept;      ///< Block-compressed (BC*).
/// Bytes of one tightly packed mip level of `width` x `height` texels (blocks for compressed formats).
std::uint64_t texture_data_size(Format format, std::uint32_t width, std::uint32_t height) noexcept;
bool is_depth(Format format) noexcept;

enum class BufferUsage : std::uint8_t {
    None = 0,
    Storage = 1 << 0,  ///< Readable/writable from shaders via buffer_address().
    Index = 1 << 1,
    Indirect = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4,
};

enum class TextureUsage : std::uint8_t {
    None = 0,
    Sampled = 1 << 0,     ///< Gets a bindless sampled-texture index.
    Storage = 1 << 1,     ///< Gets a bindless storage-image index (RGBA8 in shaders for now).
    ColorTarget = 1 << 2,
    DepthTarget = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};

enum class MemoryUsage : std::uint8_t {
    GpuOnly,  ///< Fastest for the GPU; CPU writes go through a staging copy.
    CpuToGpu, ///< Host-visible, for per-frame uploads.
    GpuToCpu, ///< Host-visible and cached, for readback.
};

/// What a texture is about to be used for. CommandList::transition() emits the barrier.
enum class ResourceState : std::uint8_t {
    Undefined,
    ColorTarget,
    DepthTarget,
    ShaderRead,
    ShaderWrite,
    TransferSrc,
    TransferDst,
    Present,
};

enum class LoadOp : std::uint8_t { Load, Clear, DontCare };
enum class StoreOp : std::uint8_t { Store, DontCare };
enum class Topology : std::uint8_t { TriangleList, TriangleStrip, LineList, PointList };
enum class CullMode : std::uint8_t { None, Front, Back };
enum class CompareOp : std::uint8_t { Never, Less, LessEqual, Greater, GreaterEqual, Equal, Always };
enum class IndexType : std::uint8_t { Uint16, Uint32 };

/// Fixed samplers, indexable in shaders (see <astral/bindless.glsl>).
enum class Sampler : std::uint32_t { LinearClamp = 0, LinearRepeat = 1, NearestClamp = 2 };

/// Optional capabilities. Query with Device::supports() before relying on them.
enum class Feature : std::uint8_t {
    BufferDeviceAddress, ///< Always present on a created device.
    Bindless,            ///< Always present on a created device.
    Validation,          ///< Validation layer active.
    Presentation,        ///< Device can present to a window surface.
    RayTracing,  ///< Acceleration structures + ray queries in any shader (GLSL_EXT_ray_query).
    TextureCompressionBC, ///< BC5/BC7 textures can be sampled (every desktop GPU).
    Timestamps,           ///< GPU timestamp queries (profiling).
    MeshShaders,
};

// --- Descriptions ----------------------------------------------------------------------
struct Extent2D {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct BufferDesc {
    std::uint64_t size = 0;
    BufferUsage usage = BufferUsage::Storage;
    MemoryUsage memory = MemoryUsage::GpuOnly;
    std::string_view debug_name = {};
};

struct TextureDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    Format format = Format::RGBA8Unorm;
    TextureUsage usage = TextureUsage::Sampled;
    std::uint32_t mip_levels = 1;
    std::string_view debug_name = {};
};

/// A compiled SPIR-V module (see astral_add_shaders) and its entry point.
struct ShaderBlob {
    std::span<const std::uint32_t> spirv;
    std::string_view entry = "main";
};

struct ComputePipelineDesc {
    ShaderBlob shader;
    std::string_view debug_name = {};
};

struct DepthState {
    Format format = Format::Undefined; ///< Undefined = no depth attachment.
    bool test = true;
    bool write = true;
    CompareOp compare = CompareOp::Less;
};

/// Graphics pipelines have no vertex input: vertex shaders pull their data from
/// buffers via device addresses ("vertex pulling").
struct GraphicsPipelineDesc {
    ShaderBlob vertex;
    ShaderBlob fragment;
    std::vector<Format> color_formats = {};
    DepthState depth = {};
    Topology topology = Topology::TriangleList;
    CullMode cull = CullMode::None;
    bool front_counter_clockwise = true;
    bool alpha_blend = false;
    std::string_view debug_name = {};
};

/// Triangle geometry for a bottom-level acceleration structure. Vertex positions are three
/// floats at the start of each `vertex_stride`-byte vertex; indices are uint32 triangles.
struct BlasDesc {
    BufferHandle vertices;
    std::uint32_t vertex_count = 0;
    std::uint32_t vertex_stride = 12;
    BufferHandle indices;
    std::uint32_t index_count = 0;
    std::string_view debug_name = {};
};

struct TlasDesc {
    std::uint32_t max_instances = 0; ///< Upper bound for CommandList::build_tlas.
    std::string_view debug_name = {};
};

/// One instance of a BLAS in a TLAS.
struct TlasInstance {
    AccelerationStructureHandle blas;
    /// Object-to-world, row-major 3x4 (rows of the upper 3x4 of the 4x4 matrix).
    std::array<float, 12> transform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    std::uint32_t custom_index = 0; ///< 24 bits, readable in shaders (e.g. a material index).
    std::uint8_t mask = 0xFF;
};

/// Push constants are limited to this many bytes in every pipeline.
inline constexpr std::uint32_t max_push_constant_size = 128;

} // namespace astral::gpu
