#include <astral/render/render_graph.hpp>

#include <array>

#include <string>
#include <vector>

namespace astral {

namespace {

enum class Use : std::uint8_t { Read, Storage, Color, Depth, DepthRead, CopySrc, CopyDst };

struct Access {
    std::uint32_t resource;
    Use use;
    gpu::LoadOp load = gpu::LoadOp::Clear;
    std::array<float, 4> clear{};
    float depth_clear = 1.0f;
};

struct Pass {
    std::string name;
    std::vector<Access> accesses;
    std::function<void(PassContext&)> execute;
    bool side_effect = false;
    bool alive = false;
};

struct Resource {
    std::string name;
    RGTextureDesc desc;
    gpu::TextureHandle texture;   // imported, or resolved from the pool
    bool imported = false;
    gpu::ResourceState final_state = gpu::ResourceState::Undefined;
    gpu::TextureUsage usage = gpu::TextureUsage::None; // union over the frame's passes
};

struct PoolEntry {
    RGTextureDesc desc;
    gpu::TextureUsage usage;
    gpu::TextureHandle texture;
    std::uint64_t last_frame = 0;
    bool taken = false;
};

gpu::ResourceState state_for(Use use) {
    switch (use) {
    case Use::Read:
    case Use::DepthRead: return gpu::ResourceState::ShaderRead;
    case Use::Storage: return gpu::ResourceState::ShaderWrite;
    case Use::Color: return gpu::ResourceState::ColorTarget;
    case Use::Depth: return gpu::ResourceState::DepthTarget;
    case Use::CopySrc: return gpu::ResourceState::TransferSrc;
    case Use::CopyDst: return gpu::ResourceState::TransferDst;
    }
    return gpu::ResourceState::Undefined;
}

gpu::TextureUsage usage_for(Use use) {
    switch (use) {
    case Use::Read: return gpu::TextureUsage::Sampled;
    case Use::Storage: return gpu::TextureUsage::Storage;
    case Use::Color: return gpu::TextureUsage::ColorTarget;
    case Use::Depth: return gpu::TextureUsage::DepthTarget;
    case Use::DepthRead: return gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled;
    case Use::CopySrc: return gpu::TextureUsage::TransferSrc;
    case Use::CopyDst: return gpu::TextureUsage::TransferDst;
    }
    return gpu::TextureUsage::None;
}

bool writes(const Access& a) {
    return a.use == Use::Storage || a.use == Use::Color || a.use == Use::Depth || a.use == Use::CopyDst;
}
bool reads(const Access& a) {
    return a.use == Use::Read || a.use == Use::DepthRead || a.use == Use::CopySrc ||
           ((a.use == Use::Color || a.use == Use::Depth) && a.load == gpu::LoadOp::Load);
}

constexpr std::uint64_t pool_keep_frames = 3; // unused pool textures are released after this

} // namespace

struct RenderGraph::Impl {
    gpu::Device& device;
    std::vector<Resource> resources;
    std::vector<Pass> passes;
    std::vector<PoolEntry> pool;
    std::uint64_t frame = 0;
    Stats stats;

    // Profiling: a ring of timestamp pools, read back (never waited for) when the slot comes round.
    static constexpr std::size_t timing_ring = 4;
    static constexpr std::uint32_t max_timed_passes = 128;
    struct TimingSlot {
        gpu::QueryPoolHandle pool;
        std::vector<std::string> names;
        bool pending = false;
    };
    std::array<TimingSlot, timing_ring> timing_slots;
    std::size_t timing_slot = 0;
    bool profiling = true;
    std::vector<PassTiming> timings;
    float timing_total = 0.0f;
    std::vector<std::uint64_t> stamps;

    void collect(TimingSlot& t) {
        if (!t.pending || t.names.empty()) return;
        stamps.resize(t.names.size() + 1);
        if (!device.read_timestamps(t.pool, 0, static_cast<std::uint32_t>(stamps.size()), stamps.data())) return;
        t.pending = false;
        timings.clear();
        for (std::size_t i = 0; i < t.names.size(); ++i) {
            timings.push_back({t.names[i], static_cast<float>(static_cast<double>(stamps[i + 1] - stamps[i]) * 1e-6)});
        }
        timing_total = static_cast<float>(static_cast<double>(stamps.back() - stamps.front()) * 1e-6);
    }

    explicit Impl(gpu::Device& d) : device(d) {}
    void add(void* pass, RGTexture t, Access a) {
        if (!t.valid() || t.index >= resources.size()) {
            device.report_error("RenderGraph: invalid texture in pass setup");
            return;
        }
        auto* p = static_cast<Pass*>(pass);
        for (const Access& existing : p->accesses) {
            if (existing.resource == t.index) {
                // One state per texture per pass: read+write would put the image in a layout
                // the other access does not expect. Use two passes (or a ping-pong texture).
                device.report_error("RenderGraph: pass '" + p->name + "' uses texture '" +
                                    resources[t.index].name + "' twice; access ignored");
                return;
            }
        }
        a.resource = t.index;
        p->accesses.push_back(a);
    }
    gpu::TextureHandle acquire(const Resource& r) {
        for (auto& e : pool) {
            if (!e.taken && e.desc.width == r.desc.width && e.desc.height == r.desc.height &&
                e.desc.format == r.desc.format && e.usage == r.usage) {
                e.taken = true;
                e.last_frame = frame;
                return e.texture;
            }
        }
        PoolEntry e{r.desc, r.usage,
                    device.create_texture({.width = r.desc.width, .height = r.desc.height,
                                           .format = r.desc.format, .usage = r.usage, .debug_name = r.name}),
                    frame, true};
        pool.push_back(e);
        return e.texture;
    }

    void cull_and_allocate() {
        // 1. Cull backwards: a pass lives if it writes something needed (an import or a later
        //    live pass's input) or has a side effect; its inputs then become needed.
        std::vector<bool> needed(resources.size(), false);
        for (std::size_t i = 0; i < resources.size(); ++i) needed[i] = resources[i].imported;
        for (auto it = passes.rbegin(); it != passes.rend(); ++it) {
            Pass& p = *it;
            p.alive = p.side_effect;
            for (const Access& a : p.accesses) {
                if (writes(a) && needed[a.resource]) p.alive = true;
            }
            if (!p.alive) continue;
            for (const Access& a : p.accesses) {
                if (reads(a)) needed[a.resource] = true;
            }
        }
        // 2. Usage per resource (over live passes), then bind pool textures.
        for (const Pass& p : passes) {
            if (!p.alive) continue;
            for (const Access& a : p.accesses) {
                auto& r = resources[a.resource];
                r.usage = r.usage | usage_for(a.use);
            }
        }
        for (auto& r : resources) {
            if (!r.imported && r.usage != gpu::TextureUsage::None) r.texture = acquire(r);
        }
    }

    TimingSlot* begin_timing(gpu::CommandList& cmd) {
        if (!profiling || !device.supports(gpu::Feature::Timestamps)) return nullptr;
        timing_slot = (timing_slot + 1) % timing_ring;
        TimingSlot* timing = &timing_slots[timing_slot];
        if (!timing->pool.valid()) timing->pool = device.create_timestamp_pool(max_timed_passes + 1);
        collect(*timing); // this slot's previous frame, if the GPU finished it
        // Any slot that finished since then too: the newest available result wins.
        for (std::size_t k = 1; k < timing_ring; ++k) collect(timing_slots[(timing_slot + k) % timing_ring]);
        if (!timing->pool.valid()) return nullptr;
        timing->names.clear();
        timing->pending = false;
        cmd.reset_queries(timing->pool, 0, max_timed_passes + 1);
        cmd.write_timestamp(timing->pool, 0);
        return timing;
    }

    void record_pass(gpu::CommandList& cmd, Pass& p, RenderGraph* graph) {
        cmd.begin_debug_label(p.name.c_str());
        gpu::RenderingDesc rendering;
        gpu::Extent2D extent{};
        for (const Access& a : p.accesses) {
            const Resource& r = resources[a.resource];
            cmd.transition(r.texture, state_for(a.use));
            if (extent.width == 0 && (writes(a) || a.use == Use::Read)) extent = {r.desc.width, r.desc.height};
            if (a.use == Use::Color) {
                rendering.color.push_back({.texture = r.texture, .load = a.load, .clear = a.clear});
                extent = {r.desc.width, r.desc.height};
            } else if (a.use == Use::Depth || a.use == Use::DepthRead) {
                rendering.depth = {.texture = r.texture, .load = a.load, .clear = a.depth_clear,
                                   .read_only = a.use == Use::DepthRead};
                extent = {r.desc.width, r.desc.height};
            }
        }
        const bool raster = !rendering.color.empty() || rendering.depth.texture.valid();
        if (raster) cmd.begin_rendering(rendering);
        PassContext ctx{cmd, extent, graph};
        if (p.execute) p.execute(ctx);
        if (raster) cmd.end_rendering();
        cmd.end_debug_label();
    }

    void prune_pool() {
        // Release pool textures unused for a while (destruction is deferred by the device).
        std::erase_if(pool, [&](const PoolEntry& e) {
            // Entries not used this frame whose size matches nothing in it (e.g. while the window
            // is being resized) go right away; others after a few idle frames.
            bool size_in_use = false;
            for (const auto& r : resources) {
                size_in_use |= !r.imported && r.desc.width == e.desc.width && r.desc.height == e.desc.height;
            }
            const bool stale = e.last_frame != frame && !size_in_use;
            if (!stale && frame - e.last_frame <= pool_keep_frames) return false;
            device.destroy(e.texture);
            return true;
        });
        stats.transient_textures = static_cast<std::uint32_t>(pool.size());
    }
};

// Builder forwards to the graph through a thread-local-free back pointer stored next to the pass.
namespace {
struct BuilderTarget {
    RenderGraph::Impl* graph;
    Pass* pass;
};
} // namespace

void PassBuilder::read(RGTexture t) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::Read});
}
void PassBuilder::write_storage(RGTexture t) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::Storage});
}
void PassBuilder::color(RGTexture t, gpu::LoadOp load, std::array<float, 4> clear) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::Color, load, clear});
}
void PassBuilder::depth(RGTexture t, gpu::LoadOp load, float clear) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::Depth, load, {}, clear});
}
void PassBuilder::depth_read(RGTexture t) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::DepthRead, gpu::LoadOp::Load});
}
void PassBuilder::copy_src(RGTexture t) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::CopySrc});
}
void PassBuilder::copy_dst(RGTexture t) {
    auto* b = static_cast<BuilderTarget*>(pass_);
    b->graph->add(b->pass, t, {0, Use::CopyDst});
}
void PassBuilder::side_effect() noexcept { static_cast<BuilderTarget*>(pass_)->pass->side_effect = true; }

gpu::TextureHandle PassContext::texture(RGTexture t) const {
    return static_cast<const RenderGraph*>(graph)->texture(t);
}
std::uint32_t PassContext::bindless(RGTexture t) const {
    const auto* g = static_cast<const RenderGraph*>(graph);
    return g->impl_bindless(t);
}

RenderGraph::RenderGraph(gpu::Device& device) : impl_(std::make_unique<Impl>(device)) {}

RenderGraph::~RenderGraph() {
    for (auto& e : impl_->pool) impl_->device.destroy(e.texture);
    for (auto& t : impl_->timing_slots) impl_->device.destroy(t.pool);
}

std::span<const PassTiming> RenderGraph::gpu_timings() const noexcept { return impl_->timings; }
float RenderGraph::gpu_frame_ms() const noexcept { return impl_->timing_total; }
void RenderGraph::set_profiling(bool enabled) noexcept {
    impl_->profiling = enabled;
    if (!enabled) {
        impl_->timings.clear();
        impl_->timing_total = 0.0f;
    }
}

void RenderGraph::begin_frame() {
    auto& m = *impl_;
    m.resources.clear();
    m.passes.clear();
    ++m.frame;
    for (auto& e : m.pool) e.taken = false;
}

RGTexture RenderGraph::import(gpu::TextureHandle texture, std::string_view name, gpu::ResourceState final_state) {
    auto& m = *impl_;
    const gpu::TextureDesc d = m.device.texture_desc(texture);
    Resource r;
    r.name = std::string(name);
    r.desc = {d.width, d.height, d.format};
    r.texture = texture;
    r.imported = true;
    r.final_state = final_state;
    m.resources.push_back(std::move(r));
    return {static_cast<std::uint32_t>(m.resources.size() - 1)};
}

RGTexture RenderGraph::create(std::string_view name, const RGTextureDesc& desc) {
    auto& m = *impl_;
    Resource r;
    r.name = std::string(name);
    r.desc = desc;
    m.resources.push_back(std::move(r));
    return {static_cast<std::uint32_t>(m.resources.size() - 1)};
}

void RenderGraph::add_pass(std::string_view name, const std::function<void(PassBuilder&)>& setup,
                           std::function<void(PassContext&)> execute) {
    auto& m = *impl_;
    m.passes.push_back({std::string(name), {}, std::move(execute)});
    BuilderTarget target{&m, &m.passes.back()};
    PassBuilder builder(&target);
    if (setup) setup(builder);
}

void RenderGraph::execute(gpu::CommandList& cmd) {
    auto& m = *impl_;
    m.stats.passes_executed = 0;
    m.stats.passes_culled = 0;

    m.cull_and_allocate();
    Impl::TimingSlot* timing = m.begin_timing(cmd);
    for (Pass& p : m.passes) {
        if (!p.alive) {
            ++m.stats.passes_culled;
            continue;
        }
        m.record_pass(cmd, p, this);
        ++m.stats.passes_executed;
        if (timing && timing->names.size() < Impl::max_timed_passes) {
            timing->names.push_back(p.name);
            cmd.write_timestamp(timing->pool, static_cast<std::uint32_t>(timing->names.size()));
        }
    }
    if (timing) timing->pending = !timing->names.empty();

    for (const auto& r : m.resources) {
        if (r.imported && r.final_state != gpu::ResourceState::Undefined) cmd.transition(r.texture, r.final_state);
    }
    m.prune_pool();
}

gpu::TextureHandle RenderGraph::texture(RGTexture t) const {
    return t.valid() && t.index < impl_->resources.size() ? impl_->resources[t.index].texture
                                                          : gpu::TextureHandle{};
}

const RenderGraph::Stats& RenderGraph::stats() const noexcept { return impl_->stats; }

std::uint32_t RenderGraph::impl_bindless(RGTexture t) const {
    return impl_->device.bindless_index(texture(t));
}

} // namespace astral
