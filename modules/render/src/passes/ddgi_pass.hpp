#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/core/math.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <array>
#include <cstdint>

namespace astral {

struct DdgiFrameInfo {
    Vec3 origin{};
    Vec3 spacing{1, 1, 1};
    std::uint32_t nx = 0, ny = 0, nz = 0, rays = 0;
    bool valid = false;
};

struct DdgiOutput {
    RGTexture irradiance{};
    RGTexture distance{};
};

/// Dynamic diffuse global illumination: probe grid management, ray tracing, and atlas updates.
struct DdgiPass {
    explicit DdgiPass(gpu::Device& device) noexcept : device_(device) {}
    ~DdgiPass();
    DdgiPass(const DdgiPass&) = delete;
    DdgiPass& operator=(const DdgiPass&) = delete;

    [[nodiscard]] bool ensure_pipelines();
    void destroy_pipelines();

    void ensure(const RenderScene& scene, const MeshStore& meshes, const GiSettings& gi);
    void reset() noexcept { valid_ = false; }
    void mark_valid() noexcept { valid_ = true; }

    [[nodiscard]] DdgiFrameInfo info() const noexcept {
        return {origin_, spacing_, nx_, ny_, nz_, rays_, valid_};
    }
    [[nodiscard]] std::uint32_t probe_count() const noexcept { return nx_ * ny_ * nz_; }

    [[nodiscard]] DdgiOutput add(RenderGraph& graph, const FrameInputs& inputs,
                                 gpu::PipelineHandle trace_pipeline) const;

private:
    void destroy_state();

    gpu::Device& device_;
    gpu::PipelineHandle update_pipeline_{};
    std::uint32_t nx_ = 0, ny_ = 0, nz_ = 0, rays_ = 0;
    Vec3 origin_{};
    Vec3 spacing_{1, 1, 1};
    std::array<gpu::TextureHandle, 2> irradiance_{};
    std::array<gpu::TextureHandle, 2> distance_{};
    gpu::BufferHandle ray_buffer_{};
    bool valid_ = false; ///< Atlases hold earlier frames' results.
};

} // namespace astral
