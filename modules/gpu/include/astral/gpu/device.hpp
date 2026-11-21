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
    /// Receives a VkInstance (as void*), returns a VkSurfaceKHR (as uint64_t), 0 on failure.
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
    [[nodiscard]] bool supports(Feature feature) const noexcept;

    // Resources ------------------------------------------------------------------------
    BufferHandle create_buffer(const BufferDesc& desc);
    TextureHandle create_texture(const TextureDesc& desc);
    PipelineHandle create_compute_pipeline(const ComputePipelineDesc& desc);
    PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc& desc);

    /// Destruction is deferred until the GPU finished every submission that may use it.
    void destroy(BufferHandle buffer);
    void destroy(TextureHandle texture);
    void destroy(PipelineHandle pipeline);

    /// Copies CPU data into a buffer (mapped write or staged copy). Blocks for staged copies.
    bool write_buffer(BufferHandle buffer, const void* data, std::uint64_t size,
                      std::uint64_t offset = 0);
    /// Copies buffer contents to the CPU. Waits for pending GPU work first.
    bool read_buffer(BufferHandle buffer, void* out, std::uint64_t size, std::uint64_t offset = 0);
    /// Uploads tightly packed texels to mip 0 and leaves the texture in ShaderRead state.
    bool write_texture(TextureHandle texture, const void* data, std::uint64_t size);

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
    explicit Device(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral::gpu
