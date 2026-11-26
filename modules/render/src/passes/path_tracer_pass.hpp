#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace astral {

/// Unbiased progressive multi-bounce Path Tracer reference pass (M13 / ADR-0033).
/// Owns a ping-pong RGBA32Float accumulation buffer pair and tracks sample count.
struct PathTracerPass {
    explicit PathTracerPass(gpu::Device& device) noexcept : device_(device) {}
    ~PathTracerPass();
    PathTracerPass(const PathTracerPass&) = delete;
    PathTracerPass& operator=(const PathTracerPass&) = delete;

    /// Recreates the RGBA32Float accumulation buffers if dimensions changed. Returns true when resized.
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);
    /// Restarts the accumulation. Several resets before the next report keep the first cause.
    void reset(PathTracerReset reason = PathTracerReset::Resources) noexcept {
        sample_index_ = 0;
        ++generation_;
        if (reset_reason_ == PathTracerReset::None) reset_reason_ = reason;
    }
    /// The cause of the resets since the last call (None if there were none), and forgets it.
    [[nodiscard]] PathTracerReset take_reset_reason() noexcept {
        return std::exchange(reset_reason_, PathTracerReset::None);
    }
    [[nodiscard]] std::uint32_t sample_count() const noexcept { return sample_index_; }
    /// The accumulation generation a recorded sample belongs to (reset() and resize() start a new one).
    [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }
    /// The recorded sample was submitted: the next frame accumulates on top of it. A sample from an
    /// older generation (history reset since) or one never submitted does not count.
    void advance(std::uint32_t generation) noexcept {
        if (generation == generation_) ++sample_index_;
    }

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, gpu::PipelineHandle pipeline);

private:
    gpu::Device& device_;
    std::uint32_t width_ = 0, height_ = 0;
    std::uint32_t sample_index_ = 0;
    std::uint32_t generation_ = 0;
    PathTracerReset reset_reason_ = PathTracerReset::None;
    std::array<gpu::TextureHandle, 2> accum_{};
};

} // namespace astral
