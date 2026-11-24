#include "passes/particle_pass.hpp"

#include <shaders/particle.frag.hpp>
#include <shaders/particle.vert.hpp>
#include <shaders/particle_simulate.comp.hpp>

#include <algorithm>
#include <cmath>

namespace astral {
ParticlePass::ParticlePass(gpu::Device& device, gpu::Format format) : device_(device) {
    pipeline_ = device.create_graphics_pipeline({
        .vertex = {shaders::particle_vert}, .fragment = {shaders::particle_frag}, .color_formats = {format},
        .depth = {.format = gpu::Format::D32Float, .test = true, .write = false},
        .additive_blend = true, .debug_name = "astral.particles",
    });
    simulation_ = device.create_compute_pipeline({.shader = {shaders::particle_simulate_comp},
                                                   .debug_name = "astral.particles.simulate"});
}
ParticlePass::~ParticlePass() {
    for (auto& entry : states_) device_.destroy(entry.second.buffer);
    device_.destroy(simulation_);
    device_.destroy(pipeline_);
}

std::vector<ParticlePass::Draw> ParticlePass::collect(std::span<const ParticleBurst> emitters) {
    ++serial_;
    std::erase_if(states_, [this](const auto& entry) {
        if (entry.second.seen + 1 >= serial_) return false;
        device_.destroy(entry.second.buffer);
        return true;
    });
    std::vector<Draw> draws;
    std::uint32_t remaining = 8192;
    for (const auto& emitter : emitters) {
        if (remaining == 0) break;
        if (!std::isfinite(emitter.age) || !std::isfinite(emitter.duration) || !std::isfinite(emitter.size_m) ||
            emitter.age < 0 || emitter.age >= emitter.duration || emitter.size_m <= 0 || emitter.count == 0) continue;
        const auto count = std::min(remaining, std::min(emitter.count, 1024u));
        remaining -= count;
        auto& draw = draws.emplace_back(Draw{emitter, count});
        if (emitter.simulation_id == 0 || !simulation_.valid()) continue;
        auto existing = states_.find(emitter.simulation_id);
        if (existing == states_.end() && states_.size() >= 64) continue;
        auto& state = states_[emitter.simulation_id];
        if (state.seen == serial_) continue;
        draw.reset = !state.buffer.valid() || state.count != count || state.seed != emitter.seed ||
                     state.age > emitter.age || emitter.age - state.age > 8.0f / 60.0f;
        if (!state.buffer.valid() || state.count != count) {
            device_.destroy(state.buffer);
            state.buffer = device_.create_buffer({.size = count * 32u, .usage = gpu::BufferUsage::Storage,
                                                   .debug_name = "astral.particle_state"});
        }
        draw.previous_age = draw.reset ? std::max(0.0f, emitter.age - 8.0f / 60.0f) : state.age;
        draw.address = state.buffer.valid() ? device_.buffer_address(state.buffer) : 0;
        state.age = emitter.age;
        state.count = count;
        state.seed = emitter.seed;
        state.seen = serial_;
    }
    return draws;
}

void ParticlePass::simulate(RenderGraph& graph, const FrameInputs& in, const GBufferTextures& gb,
                            std::span<const Draw> draws) {
    std::vector<Draw> updates;
    for (const auto& d : draws) {
        if (d.address && (d.reset || d.previous_age < d.burst.age)) updates.push_back(d);
    }
    if (updates.empty()) return;
    graph.add_pass("GPU particle simulation", [gb](PassBuilder& b) {
        b.read(gb.depth);
        b.read(gb.normal);
        if (gb.has_sdf) {
            b.read(gb.sdf_depth);
            b.read(gb.sdf_normal);
        }
        b.side_effect(); // particle state is a persistent device-address buffer
    }, [pipeline = simulation_, frame = in.addresses.frame, gb, updates = std::move(updates)](PassContext& c) {
        c.cmd.memory_barrier();
        c.cmd.bind_pipeline(pipeline);
        struct Push {
            std::uint64_t frame, states;
            Vec4 position_age, wind_duration;
            float previous_age, restitution;
            std::uint32_t seed, count, reset, depth, normal, sdf_depth, sdf_normal;
        };
        static_assert(offsetof(Push, sdf_normal) == 80);
        for (const auto& d : updates) {
            const auto& e = d.burst;
            const float bounce = std::isfinite(e.restitution) ? std::clamp(e.restitution, 0.0f, 1.0f) : 0.0f;
            const Push push{frame, d.address, {e.position.x, e.position.y, e.position.z, e.age},
                {e.drift.x, e.drift.y, e.drift.z, e.duration}, d.previous_age, bounce, e.seed, d.count, d.reset,
                c.bindless(gb.depth), c.bindless(gb.normal),
                gb.has_sdf ? c.bindless(gb.sdf_depth) : no_texture,
                gb.has_sdf ? c.bindless(gb.sdf_normal) : no_texture};
            c.cmd.push_constants(&push, sizeof(push));
            c.cmd.dispatch((d.count + 63) / 64);
        }
        c.cmd.memory_barrier(); // publish particle positions to the subsequent vertex draws
    });
}

void ParticlePass::add(RenderGraph& graph, const FrameInputs& in, RGTexture target, const GBufferTextures& gb,
                       RGTexture fog) {
    auto draws = collect(in.settings.debug_view == DebugView::None ? in.scene.particles : std::span<const ParticleBurst>{});
    if (draws.empty() || !pipeline_.valid()) return;
    simulate(graph, in, gb, draws);
    graph.add_pass("GPU particles", [target, gb, fog](PassBuilder& b) {
        b.color(target, gpu::LoadOp::Load);
        b.depth_read(gb.depth);
        if (gb.has_sdf) b.read(gb.sdf_depth);
        if (fog.valid()) b.read(fog);
    }, [pipeline = pipeline_, &in, gb, fog, draws = std::move(draws)](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            Vec4 position_age, drift_duration, color_size;
            std::uint32_t seed, fog, sdf_depth, material, mesh_depth;
            float soft_distance_m;
            std::uint64_t states;
        };
        static_assert(sizeof(Push) == 88);
        constexpr std::uint32_t none = 0xFFFFFFFFu;
        gpu::PipelineHandle bound{};
        for (const auto& draw : draws) {
            const auto& emitter = draw.burst;
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
                gb.has_sdf ? c.bindless(gb.sdf_depth) : none, in.materials.resolve_index(emitter.material),
                c.bindless(gb.depth), emitter.soft_distance_m, draw.address};
            c.cmd.push_constants(&push, sizeof(push));
            c.cmd.draw(6, draw.count);
        }
    });
}

} // namespace astral
