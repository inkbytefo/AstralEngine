#pragma once

#include <astral/gpu/command_list.hpp>
#include <astral/gpu/types.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral {
class Logger;
}

namespace astral::gpu {

/// How a window lends itself to the GPU without either module knowing the other:
/// the platform module fills this in (e.g. Window::surface_source()).
struct SurfaceSource {
    std::vector<std::string> instance_extensions;
    /// Receives the native graphics-API instance (as void*), returns the native surface handle
    /// (as uint64_t), or 0 on failure.
    std::function<std::uint64_t(void* instance)> create_surface;
    /// Current drawable size in pixels.
    std::function<Extent2D()> drawable_size;
};

struct DeviceDesc {
    std::string_view app_name = "Astral";
    bool validation = false;            ///< Enable the validation layer if it is installed.
    const SurfaceSource* surface = nullptr; ///< Null = headless (no presentation).
    Logger* logger = nullptr;           ///< Receives validation and device messages.
};

/// The GPU: owns every resource, records command lists and submits them.
/// Single-threaded for now: call from one thread (multi-threaded recording comes later).
class Device {
public:
    /// Returns nullptr when no Vulkan 1.3 device with the required features exists.
    static std::unique_ptr<Device> create(const DeviceDesc& desc);
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] std::string_view adapter_name() const noexcept;
    [[nodiscard]] AdapterInfo adapter_info() const noexcept;
    [[nodiscard]] bool supports(Feature feature) const noexcept;
    /// Logs through the device's logger (for layers built on the device, e.g. the render graph).
    void report_error(std::string_view message) const;

    // Resources ------------------------------------------------------------------------
    BufferHandle create_buffer(const BufferDesc& desc);
    TextureHandle create_texture(const TextureDesc& desc);
    PipelineHandle create_compute_pipeline(const ComputePipelineDesc& desc);
    PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc& desc);

    /// Builds a bottom-level acceleration structure from triangles (blocking; refused while a
    /// CommandList records). Needs Feature::RayTracing.
    AccelerationStructureHandle create_blas(const BlasDesc& desc);
    /// Allocates a top-level acceleration structure; fill it with CommandList::build_tlas.
    AccelerationStructureHandle create_tlas(const TlasDesc& desc);
    /// Address for shaders: `accelerationStructureEXT(address)` (0 for an invalid handle).
    [[nodiscard]] std::uint64_t acceleration_structure_address(AccelerationStructureHandle as) const noexcept;

    /// `count` GPU timestamps (Feature::Timestamps). Write them with CommandList::write_timestamp.
    QueryPoolHandle create_timestamp_pool(std::uint32_t count);
    /// Times in nanoseconds (from an arbitrary origin) of queries [first, first + count). Never waits:
    /// false if any of them has not been written by a finished submission yet.
    bool read_timestamps(QueryPoolHandle pool, std::uint32_t first, std::uint32_t count, std::uint64_t* out_ns);

    /// Destruction is deferred until the GPU finished every submission that may use it.
    void destroy(QueryPoolHandle pool);
    void destroy(AccelerationStructureHandle as);
    void destroy(BufferHandle buffer);
    void destroy(TextureHandle texture);
    void destroy(PipelineHandle pipeline);

    /// Copies CPU data into a buffer. Host-visible buffers are written directly (the caller must
    /// not overwrite data the GPU may still be reading); GPU-only buffers go through a blocking
    /// staged copy, which is refused while a CommandList is being recorded.
    bool write_buffer(BufferHandle buffer, const void* data, std::uint64_t size,
                      std::uint64_t offset = 0);
    /// Copies buffer contents to the CPU. Waits for pending GPU work first. GPU-only buffers
    /// need a staged copy, refused while a CommandList is being recorded.
    bool read_buffer(BufferHandle buffer, void* out, std::uint64_t size, std::uint64_t offset = 0);
    /// Uploads tightly packed texels to one mip level (size = max(1, w >> mip) x max(1, h >> mip))
    /// and leaves the texture in ShaderRead state. Upload mip 0 first. Blocking; refused while a
    /// CommandList is being recorded.
    bool write_texture(TextureHandle texture, const void* data, std::uint64_t size, std::uint32_t mip = 0);

    /// GPU virtual address for shaders (0 for an invalid handle).
    [[nodiscard]] std::uint64_t buffer_address(BufferHandle buffer) const noexcept;
    /// Index into the bindless texture arrays (UINT32_MAX for an invalid handle).
    [[nodiscard]] std::uint32_t bindless_index(TextureHandle texture) const noexcept;
    [[nodiscard]] TextureDesc texture_desc(TextureHandle texture) const noexcept;
    [[nodiscard]] std::uint64_t buffer_size(BufferHandle buffer) const noexcept;

    // Work -----------------------------------------------------------------------------
    CommandList begin_commands();
    SubmitTicket submit(CommandList& commands);
    void wait(SubmitTicket ticket);
    [[nodiscard]] bool is_complete(SubmitTicket ticket) const;
    void wait_idle();

    struct Impl; ///< Backend state (Vulkan). Opaque to users.
    Impl& impl() noexcept { return *impl_; }

private:
    friend class Swapchain;
    explicit Device(std::unique_ptr<Impl> impl);
    /// Submit with extra native semaphores (used by Swapchain::present).
    SubmitTicket submit_internal(CommandList& commands, const void* waits, std::uint32_t wait_count,
                                 const void* signals, std::uint32_t signal_count);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral::gpu
