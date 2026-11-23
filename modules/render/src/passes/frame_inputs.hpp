#pragma once

#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "gpu_layouts.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/core/math.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>

namespace astral {

[[nodiscard]] inline constexpr bool is_srgb(gpu::Format format) noexcept {
    return format == gpu::Format::RGBA8Srgb || format == gpu::Format::BGRA8Srgb;
}

/// GPU device addresses of the current frame's uploaded buffers.
struct FrameAddresses {
    std::uint64_t frame = 0;
    std::uint64_t instances = 0;
    std::uint64_t materials = 0;
    std::uint64_t lights = 0;
    std::uint64_t shapes = 0;
    std::uint64_t rects = 0;
    std::uint64_t decals = 0;
    std::uint64_t params = 0;
    std::uint64_t tlas = 0;
};

/// Per-frame data shared by render passes. Must stay alive until RenderGraph::execute finishes.
struct FrameInputs {
    const CameraData& camera;
    const RenderScene& scene;
    const RenderSettings& settings;
    const MeshStore& meshes;
    const MaterialStore& materials;
    ShaderStore& shaders;
    const TextureStore& textures;
    const FrameInstances& instances;
    FrameSlot& slot;
    FrameAddresses addresses{};
    gpu::Extent2D extent{};
    gpu::Format target_format = gpu::Format::RGBA8Unorm;
    std::uint32_t frame_index = 0;
    Mat4 view_projection = Mat4::identity();
};

/// Render-graph textures produced by GBufferPass and optionally augmented by SdfPass.
struct GBufferTextures {
    RGTexture albedo{};
    RGTexture normal{};
    RGTexture emissive{};
    RGTexture depth{};
    RGTexture velocity{};
    RGTexture material_ext{};
    RGTexture sdf_albedo{};
    RGTexture sdf_normal{};
    RGTexture sdf_depth{};
    RGTexture sdf_velocity{};
    bool has_sdf = false;
};

inline void read_gbuffer(PassBuilder& builder, const GBufferTextures& gbuffer) {
    for (RGTexture t : {gbuffer.albedo, gbuffer.normal, gbuffer.emissive, gbuffer.depth, gbuffer.velocity,
                        gbuffer.material_ext}) {
        builder.read(t);
    }
    if (gbuffer.has_sdf) {
        for (RGTexture t : {gbuffer.sdf_albedo, gbuffer.sdf_normal, gbuffer.sdf_depth, gbuffer.sdf_velocity}) {
            builder.read(t);
        }
    }
}

[[nodiscard]] inline GBufferIds bindless_gbuffer(const PassContext& ctx, const GBufferTextures& gbuffer) {
    return GBufferIds{
        ctx.bindless(gbuffer.albedo),
        ctx.bindless(gbuffer.normal),
        ctx.bindless(gbuffer.emissive),
        ctx.bindless(gbuffer.depth),
        ctx.bindless(gbuffer.velocity),
        ctx.bindless(gbuffer.material_ext),
        ctx.bindless(gbuffer.has_sdf ? gbuffer.sdf_albedo : gbuffer.albedo),
        ctx.bindless(gbuffer.has_sdf ? gbuffer.sdf_normal : gbuffer.normal),
        ctx.bindless(gbuffer.has_sdf ? gbuffer.sdf_depth : gbuffer.depth),
        ctx.bindless(gbuffer.has_sdf ? gbuffer.sdf_velocity : gbuffer.velocity),
    };
}

} // namespace astral
