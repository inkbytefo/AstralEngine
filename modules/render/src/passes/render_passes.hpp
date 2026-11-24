#pragma once

#include "passes/aerial_pass.hpp"
#include "passes/bloom_pass.hpp"
#include "passes/composite_pass.hpp"
#include "passes/ddgi_pass.hpp"
#include "passes/debug_lines_pass.hpp"
#include "passes/decal_pass.hpp"
#include "passes/denoise_pass.hpp"
#include "passes/gbuffer_pass.hpp"
#include "passes/lighting_pass.hpp"
#include "passes/path_tracer_pass.hpp"
#include "passes/post_pass.hpp"
#include "passes/rt_lighting_pass.hpp"
#include "passes/sdf_pass.hpp"
#include "passes/shadow_pass.hpp"
#include "passes/sky_pass.hpp"
#include "passes/taa_pass.hpp"
#include "passes/tonemap_pass.hpp"
#include "passes/transparent_pass.hpp"
#include "passes/ui_pass.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

/// Owns all render passes used by the raster and ray-traced paths.
struct RenderPasses {
    ShadowPass shadow;
    GBufferPass gbuffer;
    SdfPass sdf;
    DecalPass decals;
    LightingPass lighting;
    DdgiPass ddgi;
    RtLightingPass rt_lighting;
    PathTracerPass path_tracer;
    DenoisePass denoise;
    CompositePass composite;
    SkyPass sky;
    AerialPass aerial;
    TaaPass taa;
    BloomPass bloom;
    TonemapPass tonemap;
    TransparentPass transparent;
    PostPass post;
    DebugLinesPass debug_lines;
    UiPass ui;

    RenderPasses(gpu::Device& device, gpu::Format target_format, const RenderSettings& settings)
        : shadow(device, settings.shadow_map_size),
          gbuffer(device),
          sdf(device),
          decals(device),
          lighting(device),
          ddgi(device),
          rt_lighting(device),
          path_tracer(device),
          denoise(device),
          composite(device),
          aerial(device),
          taa(device),
          bloom(device),
          tonemap(device, target_format),
          transparent(device),
          debug_lines(device, target_format),
          ui(device, target_format) {}

    [[nodiscard]] bool ensure_rt_pipelines() {
        const bool ok = ddgi.ensure_pipelines() && denoise.ensure_pipelines() && composite.ensure_pipelines() &&
                        taa.ensure_pipelines() && bloom.ensure_pipelines();
        if (!ok) destroy_rt_pipelines();
        return ok;
    }

    void destroy_rt_pipelines() {
        ddgi.destroy_pipelines();
        denoise.destroy_pipelines();
        composite.destroy_pipelines();
        taa.destroy_pipelines();
        bloom.destroy_pipelines();
    }
};

} // namespace astral
