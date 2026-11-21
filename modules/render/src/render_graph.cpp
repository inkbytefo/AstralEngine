#include <astral/render/render_graph.hpp>

#include <string>
#include <vector>

namespace astral {

namespace {

enum class Use : std::uint8_t { Read, Storage, Color, Depth, CopySrc, CopyDst };

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
    case Use::Read: return gpu::ResourceState::ShaderRead;
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
    case Use::CopySrc: return gpu::TextureUsage::TransferSrc;
    case Use::CopyDst: return gpu::TextureUsage::TransferDst;
    }
    return gpu::TextureUsage::None;
}

bool writes(const Access& a) {
    return a.use == Use::Storage || a.use == Use::Color || a.use == Use::Depth || a.use == Use::CopyDst;
}
bool reads(const Access& a) {
    return a.use == Use::Read || a.use == Use::CopySrc ||
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

    explicit Impl(gpu::Device& d) : device(d) {}
    void add(void* pass, RGTexture t, Access a) {
        if (!t.valid() || t.index >= resources.size()) {
            return;
        }
        a.resource = t.index;
        static_cast<Pass*>(pass)->accesses.push_back(a);
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

    // 1. Cull backwards: a pass lives if it writes something needed (an import or a later
    //    live pass's input) or has a side effect; its inputs then become needed.
    std::vector<bool> needed(m.resources.size(), false);
    for (std::size_t i = 0; i < m.resources.size(); ++i) needed[i] = m.resources[i].imported;
    for (auto it = m.passes.rbegin(); it != m.passes.rend(); ++it) {
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
    for (const Pass& p : m.passes) {
        if (!p.alive) continue;
        for (const Access& a : p.accesses) {
            auto& r = m.resources[a.resource];
            r.usage = r.usage | usage_for(a.use);
        }
    }
    for (auto& r : m.resources) {
        if (!r.imported && r.usage != gpu::TextureUsage::None) r.texture = m.acquire(r);
    }

    // 3. Record.
    for (Pass& p : m.passes) {
        if (!p.alive) {
            ++m.stats.passes_culled;
            continue;
        }
        cmd.begin_debug_label(p.name.c_str());
        gpu::RenderingDesc rendering;
        gpu::Extent2D extent{};
        for (const Access& a : p.accesses) {
            const Resource& r = m.resources[a.resource];
            cmd.transition(r.texture, state_for(a.use));
            if (extent.width == 0 && (writes(a) || a.use == Use::Read)) extent = {r.desc.width, r.desc.height};
            if (a.use == Use::Color) {
                rendering.color.push_back({.texture = r.texture, .load = a.load, .clear = a.clear});
                extent = {r.desc.width, r.desc.height};
            } else if (a.use == Use::Depth) {
                rendering.depth = {.texture = r.texture, .load = a.load, .clear = a.depth_clear};
                extent = {r.desc.width, r.desc.height};
            }
        }
        const bool raster = !rendering.color.empty() || rendering.depth.texture.valid();
        if (raster) cmd.begin_rendering(rendering);
        PassContext ctx{cmd, extent, this};
        if (p.execute) p.execute(ctx);
        if (raster) cmd.end_rendering();
        cmd.end_debug_label();
        ++m.stats.passes_executed;
    }

    for (const auto& r : m.resources) {
        if (r.imported && r.final_state != gpu::ResourceState::Undefined) cmd.transition(r.texture, r.final_state);
    }

    // 4. Release pool textures unused for a while (destruction is deferred by the device).
    std::erase_if(m.pool, [&](const PoolEntry& e) {
        if (m.frame - e.last_frame <= pool_keep_frames) return false;
        m.device.destroy(e.texture);
        return true;
    });
    m.stats.transient_textures = static_cast<std::uint32_t>(m.pool.size());
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
