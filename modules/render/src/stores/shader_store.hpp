#pragma once

#include "gpu_layouts.hpp"

#include <astral/core/handle.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>
#include <astral/render/surface_shader.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace astral {

struct RtPipelines {
    gpu::PipelineHandle rt_lighting;
    gpu::PipelineHandle ddgi_trace;
    gpu::PipelineHandle ddgi_update;
    gpu::PipelineHandle temporal;
    gpu::PipelineHandle atrous;
    gpu::PipelineHandle composite;
    gpu::PipelineHandle taa;
    gpu::PipelineHandle bloom_down;
    gpu::PipelineHandle bloom_up;
};

class ShaderStore {
public:
    ShaderStore(gpu::Device& device, gpu::Format target_format) noexcept
        : device_(device), target_format_(target_format) {}
    ~ShaderStore();
    ShaderStore(const ShaderStore&) = delete;
    ShaderStore& operator=(const ShaderStore&) = delete;

    ShaderHandle create(const SurfaceShaderBuild& build, std::string_view debug_name = {});
    ShaderHandle create(const ScreenShaderBuild& build, std::string_view debug_name = {});
    void destroy(ShaderHandle handle);

    [[nodiscard]] const GpuShader* get(ShaderHandle handle) const noexcept { return shaders_.get(handle); }
    /// Looks up a live shader by its 1-based draw key (`handle.index + 1`). Returns nullptr for 0.
    [[nodiscard]] const GpuShader* find_by_key(std::uint32_t key) noexcept;

    bool ensure_rt_pipelines();
    void destroy_rt_pipelines();
    /// Recompiles the ray-traced compute shaders when custom opaque surface shaders were added or removed.
    void rebuild_rt_surfaces_if_dirty();

    [[nodiscard]] const RtPipelines& rt_pipelines() const noexcept { return rt_; }
    [[nodiscard]] std::string_view error() const noexcept { return rt_surface_error_; }

private:
    gpu::Device& device_;
    gpu::Format target_format_;
    HandlePool<GpuShader, ShaderTag> shaders_;
    RtPipelines rt_{};
    bool rt_surfaces_dirty_ = false;
    bool rt_custom_active_ = false;
    std::string rt_surface_error_;
};

} // namespace astral
