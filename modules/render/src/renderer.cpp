#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "passes/render_passes.hpp"
#include "paths/raster_path.hpp"
#include "paths/ray_traced_path.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

struct UiOnlySlot {
    GrowBuffer vertices;
    GrowBuffer indices;
    std::shared_ptr<std::uint64_t> last_use = std::make_shared<std::uint64_t>(0);
    void destroy(gpu::Device& d) {
        vertices.destroy(d);
        indices.destroy(d);
    }
};

struct Renderer::Impl {
    gpu::Device& device;
    gpu::Format target_format;
    RenderSettings settings;
    RenderGraph graph;
    TextureStore textures;
    ShaderStore shaders;
    MeshStore meshes;
    MaterialStore materials;
    FrameInstances frame_instances;
    FrameRing ring;
    std::array<UiOnlySlot, ring_size> ui_ring;
    std::uint32_t ui_ring_index = 0;
    RenderPasses passes;
    RenderStats stats;
    TemporalState temporal;
    bool rt = false;

    Impl(gpu::Device& d, gpu::Format f, const RenderSettings& s)
        : device(d),
          target_format(f),
          settings(s),
          graph(d),
          textures(d),
          shaders(d, f),
          meshes(d),
          materials(d, textures, shaders),
          passes(d, f, s) {}

    bool enable_ray_tracing() {
        const bool ok = shaders.ensure_rt_pipelines() && passes.ensure_rt_pipelines();
        if (!ok) {
            shaders.destroy_rt_pipelines();
            passes.destroy_rt_pipelines();
        }
        return ok;
    }
};

Renderer::Renderer(gpu::Device& device, gpu::Format target_format, const RenderSettings& settings)
    : impl_(std::make_unique<Impl>(device, target_format, settings)) {
    auto& m = *impl_;
    m.rt = settings.ray_tracing && device.supports(gpu::Feature::RayTracing) && m.enable_ray_tracing();
}

Renderer::~Renderer() {
    auto& m = *impl_;
    // Destroy TLAS and per-frame buffers before member destructors tear down MeshStore (BLAS), ShaderStore,
    // TextureStore.
    m.frame_instances.tlas.reset();
    m.ring.destroy(m.device);
    for (UiOnlySlot& slot : m.ui_ring) slot.destroy(m.device);
}

bool Renderer::supports(ImageFormat format) const noexcept { return impl_->textures.supports(format); }
ShaderHandle Renderer::create_shader(const SurfaceShaderBuild& build, std::string_view debug_name) {
    return impl_->shaders.create(build, debug_name);
}
ShaderHandle Renderer::create_shader(const ScreenShaderBuild& build, std::string_view debug_name) {
    return impl_->shaders.create(build, debug_name);
}
void Renderer::destroy(ShaderHandle shader) {
    impl_->shaders.destroy(shader);
    impl_->passes.path_tracer.reset();
}
std::string_view Renderer::shader_error() const noexcept { return impl_->shaders.error(); }

TextureHandle Renderer::create_texture(const ImageData& image) { return impl_->textures.create(image); }
TextureHandle Renderer::create_render_target(std::uint32_t width, std::uint32_t height) {
    return impl_->textures.create_render_target(width, height, impl_->target_format);
}
bool Renderer::resize_render_target(TextureHandle target, std::uint32_t width, std::uint32_t height) {
    return impl_->textures.resize_render_target(target, width, height, impl_->target_format);
}
gpu::TextureHandle Renderer::native_target(TextureHandle target) const noexcept {
    return impl_->textures.native_handle(target);
}
gpu::Format Renderer::target_format() const noexcept { return impl_->target_format; }
bool Renderer::update_texture(TextureHandle texture, const ImageData& image) {
    const bool updated = impl_->textures.update(texture, image);
    if (updated) impl_->passes.path_tracer.reset();
    return updated;
}
void Renderer::destroy(TextureHandle texture) {
    impl_->textures.destroy(texture);
    impl_->passes.path_tracer.reset();
}

MeshHandle Renderer::create_mesh(const MeshData& data) { return impl_->meshes.create(data, impl_->rt); }
bool Renderer::update_mesh(MeshHandle handle, const MeshData& data) {
    const bool updated = impl_->meshes.update(handle, data, impl_->rt);
    if (updated) impl_->passes.path_tracer.reset();
    return updated;
}
void Renderer::destroy(MeshHandle handle) { impl_->meshes.destroy(handle); }

MaterialHandle Renderer::create_material(const MaterialDesc& desc) { return impl_->materials.create(desc); }
bool Renderer::update_material(MaterialHandle material, const MaterialDesc& desc) {
    const bool updated = impl_->materials.update(material, desc);
    if (updated) impl_->passes.path_tracer.reset();
    return updated;
}
void Renderer::destroy(MaterialHandle material) {
    impl_->materials.destroy(material);
    impl_->passes.path_tracer.reset();
}
const MaterialDesc* Renderer::material(MaterialHandle material) const noexcept {
    return impl_->materials.get(material);
}
MaterialHandle Renderer::default_material() const noexcept { return impl_->materials.default_material(); }

const RenderStats& Renderer::stats() const noexcept { return impl_->stats; }
std::span<const PassTiming> Renderer::gpu_timings() const noexcept { return impl_->graph.gpu_timings(); }
float Renderer::gpu_frame_ms() const noexcept { return impl_->graph.gpu_frame_ms(); }
const RenderSettings& Renderer::settings() const noexcept { return impl_->settings; }

void Renderer::set_settings(const RenderSettings& settings) {
    auto& m = *impl_;
    const RenderSettings old = m.settings;
    m.settings = settings;
    const bool want_rt = settings.ray_tracing && m.device.supports(gpu::Feature::RayTracing);
    if (want_rt != m.rt) {
        m.rt = want_rt && m.enable_ray_tracing();
        // Meshes created on the raster path have no BLAS yet.
        if (m.rt) m.meshes.ensure_blas_all();
        reset_history();
    }
    const GiSettings &a = old.gi, &b = settings.gi;
    if (a.probes_x != b.probes_x || a.probes_y != b.probes_y || a.probes_z != b.probes_z ||
        a.rays_per_probe != b.rays_per_probe || old.denoise_iterations != settings.denoise_iterations ||
        old.restir_di != settings.restir_di || old.path_tracer != settings.path_tracer ||
        old.path_tracer_max_bounces != settings.path_tracer_max_bounces || old.sky_ibl != settings.sky_ibl) {
        reset_history();
    }
    if (old.shadow_map_size != settings.shadow_map_size && settings.shadow_map_size > 0) {
        m.passes.shadow.resize(settings.shadow_map_size);
    }
}

bool Renderer::ray_tracing() const noexcept { return impl_->rt; }

void Renderer::reset_history() {
    impl_->temporal.history_valid = false;
    impl_->passes.ddgi.reset();
    impl_->passes.path_tracer.reset();
}

void Renderer::render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                      const RenderScene& scene) {
    auto& m = *impl_;
    m.stats = {};
    const gpu::TextureDesc target_desc = m.device.texture_desc(target);
    if (target_desc.width == 0) return;
    const gpu::Extent2D extent{target_desc.width, target_desc.height};
    if (m.rt) {
        RayTracedPathContext ctx{m.device, m.graph, m.meshes, m.materials, m.shaders, m.textures, m.frame_instances,
                                 m.ring,   m.passes, m.settings, m.target_format, m.temporal, m.stats};
        render_ray_traced_path(ctx, cmd, target, camera, scene, extent);
        return;
    }
    RasterPathContext ctx{m.device, m.graph, m.meshes, m.materials, m.shaders, m.textures, m.frame_instances,
                          m.ring,   m.passes, m.settings, m.target_format, m.stats};
    render_raster_path(ctx, cmd, target, camera, scene, extent);
}

void Renderer::render_ui(gpu::CommandList& cmd, gpu::TextureHandle target, std::span<const UiLayer> ui,
                         Vec4 clear_color) {
    auto& m = *impl_;
    const gpu::TextureDesc target_desc = m.device.texture_desc(target);
    if (target_desc.width == 0) return;
    UiOnlySlot& slot = m.ui_ring[m.ui_ring_index];
    m.ui_ring_index = (m.ui_ring_index + 1) % ring_size;
    m.device.wait({*slot.last_use});
    cmd.on_submitted([use = slot.last_use](gpu::SubmitTicket t) { *use = t.value; });
    m.passes.ui.record_standalone(cmd, target, {target_desc.width, target_desc.height}, m.target_format, m.textures, ui,
                                  slot.vertices, slot.indices, clear_color);
}

} // namespace astral
