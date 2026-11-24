#include "passes/particle_pass.hpp"

#include <shaders/particle.frag.hpp>
#include <shaders/particle.vert.hpp>

#include <algorithm>
#include <cmath>

namespace astral {
ParticlePass::ParticlePass(gpu::Device& device, gpu::Format format) : device_(device) {
    pipeline_ = device.create_graphics_pipeline({
        .vertex = {shaders::particle_vert}, .fragment = {shaders::particle_frag}, .color_formats = {format},
        .depth = {.format = gpu::Format::D32Float, .test = true, .write = false},
        .additive_blend = true, .debug_name = "astral.particles",
    });
}
ParticlePass::~ParticlePass() { device_.destroy(pipeline_); }

void ParticlePass::add(RenderGraph& graph, const FrameInputs& in, RGTexture target, const GBufferTextures& gb,
                       RGTexture fog) const {
    if (in.scene.particles.empty() || !pipeline_.valid() || in.settings.debug_view != DebugView::None) return;
    graph.add_pass("GPU particles", [target, gb, fog](PassBuilder& b) {
        b.color(target, gpu::LoadOp::Load);
        b.depth(gb.depth, gpu::LoadOp::Load);
        if (gb.has_sdf) b.read(gb.sdf_depth);
        if (fog.valid()) b.read(fog);
    }, [pipeline = pipeline_, &in, gb, fog](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            Vec4 position_age, drift_duration, color_size;
            std::uint32_t seed, fog, sdf_depth, material;
        };
        static_assert(sizeof(Push) == 72);
        constexpr std::uint32_t none = 0xFFFFFFFFu;
        std::uint32_t remaining = 8192;
        gpu::PipelineHandle bound{};
        for (const auto& emitter : in.scene.particles) {
            if (remaining == 0) break;
            if (!std::isfinite(emitter.age) || !std::isfinite(emitter.duration) || !std::isfinite(emitter.size_m) ||
                emitter.age < 0 || emitter.age >= emitter.duration || emitter.size_m <= 0 || emitter.count == 0) continue;
            const auto count = std::min(remaining, std::min(emitter.count, 1024u));
            const auto* shader = in.materials.shader_of(emitter.material);
            const bool custom = shader && shader->kind == GpuShader::Kind::Particle;
            const auto current = custom ? shader->forward : pipeline;
            if (bound != current) {
                c.cmd.bind_pipeline(current);
                bound = current;
            }
            const Push push{in.addresses.frame,
                {emitter.position.x, emitter.position.y, emitter.position.z, emitter.age},
                {emitter.drift.x, emitter.drift.y, emitter.drift.z, emitter.duration},
                {emitter.color.x, emitter.color.y, emitter.color.z, emitter.size_m},
                emitter.seed, fog.valid() ? c.bindless(fog) : none,
                gb.has_sdf ? c.bindless(gb.sdf_depth) : none, in.materials.resolve_index(emitter.material)};
            c.cmd.push_constants(&push, sizeof(push));
            c.cmd.draw(6, count);
            remaining -= count;
        }
    });
}

} // namespace astral
