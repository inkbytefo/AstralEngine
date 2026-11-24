#pragma once

#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace astral {

/// A texture inside one frame's graph (index into the graph's resource list).
struct RGTexture {
    std::uint32_t index = UINT32_MAX;
    [[nodiscard]] bool valid() const noexcept { return index != UINT32_MAX; }
    bool operator==(const RGTexture&) const = default;
};

/// GPU time of one pass (profiling).
struct PassTiming {
    std::string name;
    float ms = 0.0f;
};

struct RGTextureDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    gpu::Format format = gpu::Format::RGBA8Unorm;
};

/// Declares how a pass uses textures. The graph turns this into barriers, render pass
/// setup and pass culling — passes never call transition() or begin_rendering() themselves.
class PassBuilder {
public:
    /// Sampled read in a shader (bindless index via PassContext::bindless()).
    void read(RGTexture texture);
    /// Storage-image write in a compute shader.
    void write_storage(RGTexture texture);
    /// Color attachment; the graph begins rendering with all declared attachments.
    void color(RGTexture texture, gpu::LoadOp load = gpu::LoadOp::Clear,
               std::array<float, 4> clear = {0, 0, 0, 1});
    void depth(RGTexture texture, gpu::LoadOp load = gpu::LoadOp::Clear, float clear = 1.0f);
    /// Depth test and sample the same texture. The pipeline must leave depth writes disabled.
    void depth_read(RGTexture texture);
    void copy_src(RGTexture texture);
    void copy_dst(RGTexture texture);
    /// Keep this pass even if nothing reads its outputs (debug output, readbacks, ...).
    void side_effect() noexcept;

private:
    friend class RenderGraph;
    explicit PassBuilder(void* pass) noexcept : pass_(pass) {}
    void* pass_;
};

struct PassContext {
    gpu::CommandList& cmd;
    gpu::Extent2D extent; ///< Size of the attachments (or of the first written texture).
    [[nodiscard]] gpu::TextureHandle texture(RGTexture t) const;
    [[nodiscard]] std::uint32_t bindless(RGTexture t) const;

    const void* graph = nullptr; // internal
};

/// Frame graph: rebuild it every frame (begin_frame → import/create → add_pass → execute).
/// Transient textures come from a pool that persists across frames.
class RenderGraph {
public:
    explicit RenderGraph(gpu::Device& device);
    ~RenderGraph();
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    void begin_frame();
    /// An external texture (e.g. the swapchain image). Writing to it keeps passes alive.
    /// After execute() it is left in `final_state` (Undefined = wherever the last pass left it).
    RGTexture import(gpu::TextureHandle texture, std::string_view name,
                     gpu::ResourceState final_state = gpu::ResourceState::Undefined);
    /// A texture that only lives for this frame (pooled).
    RGTexture create(std::string_view name, const RGTextureDesc& desc);
    void add_pass(std::string_view name, const std::function<void(PassBuilder&)>& setup,
                  std::function<void(PassContext&)> execute);
    /// Culls, allocates, records barriers and runs every live pass into `cmd`.
    void execute(gpu::CommandList& cmd);

    /// GPU texture behind `t` (valid after execute()).
    [[nodiscard]] gpu::TextureHandle texture(RGTexture t) const;

    struct Stats {
        std::uint32_t passes_executed = 0;
        std::uint32_t passes_culled = 0;
        std::uint32_t transient_textures = 0; ///< Textures alive in the pool.
    };
    [[nodiscard]] const Stats& stats() const noexcept;

    /// GPU time per executed pass of a recent frame. Results are read a few frames later without
    /// waiting, so they lag slightly. Empty without gpu::Feature::Timestamps or when profiling is off.
    [[nodiscard]] std::span<const PassTiming> gpu_timings() const noexcept;
    /// Sum of gpu_timings() (first pass start to last pass end).
    [[nodiscard]] float gpu_frame_ms() const noexcept;
    /// On by default when the device has timestamps (cost: two queries per pass).
    void set_profiling(bool enabled) noexcept;

    struct Impl;
    /// Bindless index of `t` (used by PassContext).
    [[nodiscard]] std::uint32_t impl_bindless(RGTexture t) const;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
