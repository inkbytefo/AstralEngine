#pragma once

#include <astral/gpu/types.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <utility>
#include <vector>

namespace astral::gpu {

class Device;

struct ColorAttachment {
    TextureHandle texture;
    LoadOp load = LoadOp::Clear;
    StoreOp store = StoreOp::Store;
    std::array<float, 4> clear = {0, 0, 0, 1};
};

struct DepthAttachment {
    TextureHandle texture;
    LoadOp load = LoadOp::Clear;
    StoreOp store = StoreOp::Store;
    float clear = 1.0f;
};

struct RenderingDesc {
    std::vector<ColorAttachment> color = {};
    DepthAttachment depth = {}; ///< Leave texture invalid for no depth.
    Extent2D extent = {};       ///< Zero = size of the first attachment.
};

/// Records GPU work. Obtained from Device::begin_commands(), handed back with
/// Device::submit() (or Swapchain::present()). Texture states are tracked at record time:
/// transitions are computed from the state the previous recorded command list left the
/// texture in, so command lists must be submitted in the order they were recorded. A list
/// dropped without submitting rolls its state changes back. Misuse (invalid handles, no
/// pipeline bound, out-of-range copies, use after submit) is logged and skipped.
/// Not thread-safe.
class CommandList {
public:
    CommandList(CommandList&& other) noexcept;
    CommandList& operator=(CommandList&& other) noexcept;
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    ~CommandList();

    /// Emits a barrier moving `texture` from its tracked state to `state`.
    void transition(TextureHandle texture, ResourceState state);
    /// Full memory barrier between all previous and following commands (simple, not fast).
    void memory_barrier();

    void begin_rendering(const RenderingDesc& desc);
    void end_rendering();
    void set_viewport(float x, float y, float width, float height);
    void set_scissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height);

    void bind_pipeline(PipelineHandle pipeline);
    void push_constants(const void* data, std::uint32_t size, std::uint32_t offset = 0);
    void bind_index_buffer(BufferHandle buffer, IndexType type, std::uint64_t offset = 0);

    void draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1,
              std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0);
    void draw_indexed(std::uint32_t index_count, std::uint32_t instance_count = 1,
                      std::uint32_t first_index = 0, std::int32_t vertex_offset = 0,
                      std::uint32_t first_instance = 0);
    void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1);

    void copy_buffer(BufferHandle src, BufferHandle dst, std::uint64_t size,
                     std::uint64_t src_offset = 0, std::uint64_t dst_offset = 0);
    /// Copies mip 0 of `src` (must be in TransferSrc state) tightly packed into `dst`.
    void copy_texture_to_buffer(TextureHandle src, BufferHandle dst);
    /// Copies tightly packed texels from `src` into mip 0 of `dst` (TransferDst state).
    void copy_buffer_to_texture(BufferHandle src, TextureHandle dst);

    void begin_debug_label(const char* name);
    void end_debug_label();

    [[nodiscard]] bool valid() const noexcept { return device_ != nullptr; }

private:
    friend class Device;
    CommandList(Device* device, void* native, std::uint32_t slot) noexcept;

    bool usable() const noexcept { return device_ != nullptr; }
    void remember_state(TextureHandle texture, ResourceState previous);

    Device* device_ = nullptr;
    void* native_ = nullptr; // VkCommandBuffer
    std::uint32_t slot_ = 0;
    bool rendering_ = false;
    PipelineHandle bound_pipeline_ = {};
    bool compute_bound_ = false;
    /// First state each texture had in this list, to roll back if the list is dropped.
    std::vector<std::pair<TextureHandle, ResourceState>> touched_;
};

} // namespace astral::gpu
