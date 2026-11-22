#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "gpu_layouts.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <shaders/debug_line.frag.hpp>
#include <shaders/debug_line.vert.hpp>
#include <shaders/gbuffer.frag.hpp>
#include <shaders/gbuffer.vert.hpp>
#include <shaders/lighting.comp.hpp>
#include <shaders/sdf.comp.hpp>
#include <shaders/shadow.vert.hpp>
#include <shaders/tonemap.frag.hpp>
#include <shaders/tonemap.vert.hpp>
#include <shaders/ui.frag.hpp>
#include <shaders/ui.vert.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace astral {

namespace {

bool is_srgb(gpu::Format f) { return f == gpu::Format::RGBA8Srgb || f == gpu::Format::BGRA8Srgb; }

/// Orthographic sun view fitted tightly (in light space) around every mesh's transformed
/// bounding box and every SDF shape. Also returns the world size of one shadow texel.
template <class MeshLookup>
Mat4 light_view_projection(const RenderScene& scene, MeshLookup&& mesh_of, std::uint32_t map_size,
                           bool& valid, float& texel_world) {
    std::vector<Vec3> points;
    for (const MeshInstance& d : scene.meshes) {
        const GpuMesh* mesh = mesh_of(d.mesh);
        if (!mesh) continue;
        for (int i = 0; i < 8; ++i) {
            const Vec3 c{(i & 1) ? mesh->bounds_max.x : mesh->bounds_min.x, (i & 2) ? mesh->bounds_max.y : mesh->bounds_min.y,
                         (i & 4) ? mesh->bounds_max.z : mesh->bounds_min.z};
            const Vec4 w = d.transform * Vec4{c.x, c.y, c.z, 1};
            points.push_back({w.x, w.y, w.z});
        }
    }
    valid = !points.empty();
    if (!valid) return Mat4::identity();
    Vec3 center{};
    for (const Vec3& p : points) center += p;
    center = center / static_cast<float>(points.size());
    const Vec3 dir = normalize(scene.sun.direction);
    const Vec3 up = std::abs(dir.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Mat4 view = look_at(center - dir * 1000.0f, center, up);
    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    for (const Vec3& p : points) {
        const Vec4 v = view * Vec4{p.x, p.y, p.z, 1};
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    const float pad = 0.05f + 0.01f * std::max(hi.x - lo.x, hi.y - lo.y);
    lo = lo - Vec3{pad, pad, pad};
    hi = hi + Vec3{pad, pad, pad};
    texel_world = std::max(hi.x - lo.x, hi.y - lo.y) / static_cast<float>(map_size);
    // View space looks down -Z: near/far are the negated z bounds. Undo orthographic()'s
    // Vulkan Y flip because the shadow map is sampled with plain UVs.
    return scale({1, -1, 1}) * orthographic(lo.x, hi.x, lo.y, hi.y, -hi.z, -lo.z) * view;
}

/// Textures that live across frames (temporal history), in pairs: [frame parity].
struct HistoryTextures {
    std::uint32_t width = 0, height = 0;
    std::array<gpu::TextureHandle, 2> geometry{}, diffuse{}, specular{}, taa{}, moments{};
    void destroy(gpu::Device& device) {
        for (auto* pair : {&geometry, &diffuse, &specular, &taa, &moments}) {
            for (auto& t : *pair) device.destroy(t);
        }
        *this = {};
    }
};

struct DdgiState {
    std::uint32_t nx = 0, ny = 0, nz = 0, rays = 0;
    Vec3 origin{}, spacing{1, 1, 1};
    std::array<gpu::TextureHandle, 2> irradiance{}, distance{};
    gpu::BufferHandle ray_buffer;
    bool valid = false; ///< Atlases hold earlier frames' results.
    void destroy(gpu::Device& device) {
        for (auto& t : irradiance) device.destroy(t);
        for (auto& t : distance) device.destroy(t);
        device.destroy(ray_buffer);
        *this = {};
    }
};

float halton(std::uint32_t index, std::uint32_t base) {
    float f = 1.0f, r = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

} // namespace

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
    gpu::PipelineHandle gbuffer, shadow, sdf, lighting, tonemap, debug_depth, debug_top, ui;
    gpu::TextureHandle shadow_map;
    std::vector<UiVertex> ui_vertex_scratch;
    std::vector<std::uint32_t> ui_index_scratch;
    std::vector<float> debug_vertices; // scratch: position.xyzw, color per vertex
    RenderStats stats;

    // Ray-traced path state.
    bool rt = false;
    HistoryTextures history;
    DdgiState ddgi;
    std::uint32_t frame_index = 0;
    bool history_valid = false;
    Mat4 prev_view_projection = Mat4::identity();

    Impl(gpu::Device& d, gpu::Format f, const RenderSettings& s)
        : device(d), target_format(f), settings(s), graph(d), textures(d), shaders(d, f), meshes(d),
          materials(d, textures, shaders) {}

    void add_sky(RenderGraph& g, RGTexture hdr, RGTexture depth, std::uint64_t frame_address, std::uint64_t material_address,
                 MaterialHandle sky);
    void add_transparent(RenderGraph& g, RGTexture target, RGTexture depth, std::uint64_t instance_address,
                         std::uint64_t frame_address, std::uint64_t material_address, std::uint64_t params_address,
                         std::uint64_t light_address, float time);
    /// Runs the post chain on `scene_color`; the last effect writes `output`.
    void add_post(RenderGraph& g, RGTexture scene_color, RGTexture output, std::span<const MaterialHandle> chain,
                  std::uint32_t w, std::uint32_t h, std::uint64_t frame_address, std::uint64_t material_address);
    /// The 2D overlay over the final image.
    void add_ui(RenderGraph& g, FrameSlot& slot, RGTexture output, std::uint32_t w, std::uint32_t h,
                std::span<const UiLayer> layers);
    /// Debug lines over the final image: depth-tested ones against the scene depth, then the rest.
    void add_debug_lines(RenderGraph& g, FrameSlot& slot, RGTexture output, RGTexture depth, const Mat4& view_projection,
                         std::span<const DebugLine> lines);
    void render_ray_traced(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                           const RenderScene& scene, std::uint32_t w, std::uint32_t h);
    void ensure_history(std::uint32_t w, std::uint32_t h);
    void ensure_ddgi(const RenderScene& scene);
};

Renderer::Renderer(gpu::Device& device, gpu::Format target_format, const RenderSettings& settings)
    : impl_(std::make_unique<Impl>(device, target_format, settings)) {
    auto& m = *impl_;
    m.gbuffer = device.create_graphics_pipeline({
        .vertex = {shaders::gbuffer_vert},
        .fragment = {shaders::gbuffer_frag},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float, gpu::Format::RG16Float},
        .depth = {.format = gpu::Format::D32Float},
        .cull = gpu::CullMode::Back,
        .debug_name = "astral.gbuffer",
    });
    m.shadow = device.create_graphics_pipeline({
        .vertex = {shaders::shadow_vert},
        .fragment = {},
        .color_formats = {},
        .depth = {.format = gpu::Format::D32Float},
        .cull = gpu::CullMode::None, // both faces: no light leaking through thin geometry
        .debug_name = "astral.shadow",
    });
    m.sdf = device.create_compute_pipeline({.shader = {shaders::sdf_comp}, .debug_name = "astral.sdf"});
    m.lighting = device.create_compute_pipeline({.shader = {shaders::lighting_comp}, .debug_name = "astral.lighting"});
    m.tonemap = device.create_graphics_pipeline({
        .vertex = {shaders::tonemap_vert},
        .fragment = {shaders::tonemap_frag},
        .color_formats = {target_format},
        .depth = {.format = gpu::Format::Undefined, .test = false, .write = false},
        .debug_name = "astral.tonemap",
    });
    for (const bool on_top : {false, true}) {
        (on_top ? m.debug_top : m.debug_depth) = device.create_graphics_pipeline({
            .vertex = {shaders::debug_line_vert},
            .fragment = {shaders::debug_line_frag},
            .color_formats = {target_format},
            .depth = {.format = gpu::Format::D32Float, .test = !on_top, .write = false, .compare = gpu::CompareOp::LessEqual},
            .topology = gpu::Topology::LineList,
            .alpha_blend = true,
            .debug_name = on_top ? "astral.debug_lines_top" : "astral.debug_lines",
        });
    }
    m.ui = device.create_graphics_pipeline({
        .vertex = {shaders::ui_vert},
        .fragment = {shaders::ui_frag},
        .color_formats = {target_format},
        .depth = {.format = gpu::Format::Undefined, .test = false, .write = false},
        .alpha_blend = true,
        .debug_name = "astral.ui",
    });
    m.shadow_map = device.create_texture({.width = settings.shadow_map_size, .height = settings.shadow_map_size,
                                          .format = gpu::Format::D32Float,
                                          .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled,
                                          .debug_name = "astral.shadow_map"});
    m.rt = settings.ray_tracing && device.supports(gpu::Feature::RayTracing) && m.shaders.ensure_rt_pipelines();
}

Renderer::~Renderer() {
    auto& m = *impl_;
    // Destroy TLAS and per-frame buffers before member destructors tear down MeshStore (BLAS), ShaderStore, TextureStore.
    m.frame_instances.destroy(m.device);
    m.ring.destroy(m.device);
    for (auto p : {m.gbuffer, m.shadow, m.sdf, m.lighting, m.tonemap, m.debug_depth, m.debug_top, m.ui}) {
        m.device.destroy(p);
    }
    m.device.destroy(m.shadow_map);
    m.history.destroy(m.device);
    m.ddgi.destroy(m.device);
}

void Renderer::Impl::add_ui(RenderGraph& g, FrameSlot& slot, RGTexture output, std::uint32_t w, std::uint32_t h,
                            std::span<const UiLayer> layers) {
    std::size_t commands = 0;
    ui_vertex_scratch.clear();
    ui_index_scratch.clear();
    struct Draw {
        std::uint32_t first_index, index_count;
        std::int32_t vertex_offset;
        std::uint32_t texture; // bindless index or UINT32_MAX
        float sdf_softness;
        std::int32_t x, y;
        std::uint32_t cw, ch;
    };
    std::vector<Draw> draws;
    for (const UiLayer& layer : layers) {
        const auto vertex_base = static_cast<std::int32_t>(ui_vertex_scratch.size());
        const auto index_base = static_cast<std::uint32_t>(ui_index_scratch.size());
        ui_vertex_scratch.insert(ui_vertex_scratch.end(), layer.vertices.begin(), layer.vertices.end());
        ui_index_scratch.insert(ui_index_scratch.end(), layer.indices.begin(), layer.indices.end());
        for (const UiCommand& c : layer.commands) {
            if (c.index_count == 0 || c.first_index + c.index_count > layer.indices.size()) continue;
            const float x0 = std::clamp(c.clip.x, 0.0f, static_cast<float>(w)), y0 = std::clamp(c.clip.y, 0.0f, static_cast<float>(h));
            const float x1 = std::clamp(c.clip.z, 0.0f, static_cast<float>(w)), y1 = std::clamp(c.clip.w, 0.0f, static_cast<float>(h));
            if (x1 <= x0 || y1 <= y0) continue;
            const std::uint32_t texture = textures.bindless_or(c.texture, UINT32_MAX);
            draws.push_back({index_base + c.first_index, c.index_count, vertex_base + c.vertex_offset, texture, c.sdf_softness,
                             static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
                             static_cast<std::uint32_t>(x1 - std::floor(x0)), static_cast<std::uint32_t>(y1 - std::floor(y0))});
            ++commands;
        }
    }
    if (commands == 0) return;
    slot.ui_vertices.write(device, ui_vertex_scratch.data(), ui_vertex_scratch.size() * sizeof(UiVertex));
    slot.ui_indices.write(device, ui_index_scratch.data(), ui_index_scratch.size() * sizeof(std::uint32_t));
    const std::uint64_t vertices = device.buffer_address(slot.ui_vertices.buffer);
    const std::uint64_t indices = device.buffer_address(slot.ui_indices.buffer);
    const std::uint32_t linear_target = is_srgb(target_format) ? 2u : 0u;
    const gpu::PipelineHandle pipeline = ui;
    g.add_pass("ui", [&](PassBuilder& b) { b.color(output, gpu::LoadOp::Load); }, [=, draws = std::move(draws)](PassContext& ctx) {
        ctx.cmd.bind_pipeline(pipeline);
        for (const Draw& d : draws) {
            const UiPush push{{2.0f / static_cast<float>(w), 2.0f / static_cast<float>(h)}, {-1.0f, -1.0f}, vertices, indices,
                              d.vertex_offset, d.texture, (d.texture != UINT32_MAX ? 1u : 0u) | linear_target, d.sdf_softness};
            ctx.cmd.set_scissor(d.x, d.y, d.cw, d.ch);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(d.index_count, 1, d.first_index);
        }
    });
}

void Renderer::Impl::add_debug_lines(RenderGraph& g, FrameSlot& slot, RGTexture output, RGTexture depth,
                                     const Mat4& view_projection, std::span<const DebugLine> lines) {
    if (lines.empty()) return;
    debug_vertices.clear();
    std::uint32_t tested = 0;
    for (const bool on_top : {false, true}) { // depth-tested lines first, then the rest
        for (const DebugLine& l : lines) {
            if (l.on_top != on_top) continue;
            for (const Vec3& p : {l.a, l.b}) {
                debug_vertices.insert(debug_vertices.end(), {p.x, p.y, p.z, 1.0f, l.color.x, l.color.y, l.color.z, l.color.w});
            }
            if (!on_top) tested += 2;
        }
    }
    const auto total = static_cast<std::uint32_t>(debug_vertices.size() / 8);
    slot.debug_lines.write(device, debug_vertices.data(), debug_vertices.size() * sizeof(float));
    const DebugLinePush push{view_projection, device.buffer_address(slot.debug_lines.buffer)};
    const gpu::PipelineHandle depth_pipeline = debug_depth, top_pipeline = debug_top;
    g.add_pass("debug.lines", [&](PassBuilder& b) {
        b.color(output, gpu::LoadOp::Load);
        b.depth(depth, gpu::LoadOp::Load);
    }, [=](PassContext& ctx) {
        if (tested) {
            ctx.cmd.bind_pipeline(depth_pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(tested);
        }
        if (total > tested) {
            ctx.cmd.bind_pipeline(top_pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(total - tested, 1, tested);
        }
    });
}

void Renderer::Impl::add_sky(RenderGraph& g, RGTexture hdr, RGTexture depth, std::uint64_t frame_address,
                             std::uint64_t material_address, MaterialHandle sky) {
    const GpuShader* gs = materials.shader_of(sky);
    if (!gs || gs->kind != GpuShader::Kind::Sky) return;
    const gpu::PipelineHandle pipeline = gs->forward;
    const std::uint32_t index = sky.index;
    g.add_pass("sky", [&](PassBuilder& b) {
        b.color(hdr, gpu::LoadOp::Load);
        b.depth(depth, gpu::LoadOp::Load); // only where nothing was drawn (depth 1)
    }, [=](PassContext& ctx) {
        const ScreenPush push{frame_address, material_address, index, 0};
        ctx.cmd.bind_pipeline(pipeline);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.draw(3);
    });
}

void Renderer::Impl::add_transparent(RenderGraph& g, RGTexture target, RGTexture depth, std::uint64_t instance_address,
                                     std::uint64_t frame_address, std::uint64_t material_address, std::uint64_t params_address,
                                     std::uint64_t light_address, float time) {
    if (frame_instances.transparent_draws.empty()) return;
    g.add_pass("transparent", [&](PassBuilder& b) {
        b.color(target, gpu::LoadOp::Load);
        b.depth(depth, gpu::LoadOp::Load);
    }, [=, this](PassContext& ctx) {
        ShaderHandle bound{};
        for (const TransparentDraw& draw : frame_instances.transparent_draws) {
            const GpuShader* gs = shaders.get(draw.shader);
            const GpuMesh* mesh = meshes.get(draw.mesh);
            if (!gs || !mesh) continue;
            if (draw.shader != bound) {
                ctx.cmd.bind_pipeline(gs->forward);
                bound = draw.shader;
            }
            const MeshPush push{device.buffer_address(mesh->vertices), instance_address, frame_address, material_address,
                                draw.instance, params_address, time, light_address};
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
            ctx.cmd.draw_indexed(mesh->index_count, 1);
            ++stats.draw_calls;
            ++stats.instances;
        }
    });
}

void Renderer::Impl::add_post(RenderGraph& g, RGTexture scene_color, RGTexture output, std::span<const MaterialHandle> chain,
                              std::uint32_t w, std::uint32_t h, std::uint64_t frame_address, std::uint64_t material_address) {
    RGTexture source = scene_color;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        const GpuShader* gs = materials.shader_of(chain[i]);
        if (!gs) continue;
        const bool last = i + 1 == chain.size();
        const RGTexture target = last ? output : g.create("post", {w, h, target_format});
        const gpu::PipelineHandle pipeline = gs->forward;
        const std::uint32_t index = chain[i].index;
        const RGTexture src = source;
        g.add_pass("post", [&, src, target](PassBuilder& b) {
            b.read(src);
            b.color(target, gpu::LoadOp::DontCare);
        }, [=](PassContext& ctx) {
            const ScreenPush push{frame_address, material_address, index, ctx.bindless(src)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(3);
        });
        source = target;
    }
}

bool Renderer::supports(ImageFormat format) const noexcept { return impl_->textures.supports(format); }
ShaderHandle Renderer::create_shader(const SurfaceShaderBuild& build, std::string_view debug_name) {
    return impl_->shaders.create(build, debug_name);
}
ShaderHandle Renderer::create_shader(const ScreenShaderBuild& build, std::string_view debug_name) {
    return impl_->shaders.create(build, debug_name);
}
void Renderer::destroy(ShaderHandle shader) { impl_->shaders.destroy(shader); }
std::string_view Renderer::shader_error() const noexcept { return impl_->shaders.error(); }

TextureHandle Renderer::create_texture(const ImageData& image) { return impl_->textures.create(image); }
bool Renderer::update_texture(TextureHandle texture, const ImageData& image) { return impl_->textures.update(texture, image); }
void Renderer::destroy(TextureHandle texture) { impl_->textures.destroy(texture); }

MeshHandle Renderer::create_mesh(const MeshData& data) { return impl_->meshes.create(data, impl_->rt); }
bool Renderer::update_mesh(MeshHandle handle, const MeshData& data) { return impl_->meshes.update(handle, data, impl_->rt); }
void Renderer::destroy(MeshHandle handle) { impl_->meshes.destroy(handle); }

MaterialHandle Renderer::create_material(const MaterialDesc& desc) { return impl_->materials.create(desc); }
bool Renderer::update_material(MaterialHandle material, const MaterialDesc& desc) { return impl_->materials.update(material, desc); }
void Renderer::destroy(MaterialHandle material) { impl_->materials.destroy(material); }
const MaterialDesc* Renderer::material(MaterialHandle material) const noexcept { return impl_->materials.get(material); }
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
        m.rt = want_rt && m.shaders.ensure_rt_pipelines();
        // Meshes created on the raster path have no BLAS yet.
        if (m.rt) m.meshes.ensure_blas_all();
        reset_history();
    }
    const GiSettings &a = old.gi, &b = settings.gi;
    if (a.probes_x != b.probes_x || a.probes_y != b.probes_y || a.probes_z != b.probes_z ||
        a.rays_per_probe != b.rays_per_probe || old.denoise_iterations != settings.denoise_iterations) {
        reset_history();
    }
    if (old.shadow_map_size != settings.shadow_map_size && settings.shadow_map_size > 0) {
        m.device.destroy(m.shadow_map);
        m.shadow_map = m.device.create_texture({.width = settings.shadow_map_size, .height = settings.shadow_map_size,
                                                .format = gpu::Format::D32Float,
                                                .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled,
                                                .debug_name = "astral.shadow_map"});
    }
}

bool Renderer::ray_tracing() const noexcept { return impl_->rt; }

void Renderer::reset_history() {
    impl_->history_valid = false;
    impl_->ddgi.valid = false;
}

void Renderer::render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                      const RenderScene& scene) {
    auto& m = *impl_;
    m.stats = {};
    const gpu::TextureDesc target_desc = m.device.texture_desc(target);
    if (target_desc.width == 0) return;
    const std::uint32_t w = target_desc.width, h = target_desc.height;
    if (m.rt) {
        m.render_ray_traced(cmd, target, camera, scene, w, h);
        return;
    }

    // --- Per-frame data into this ring slot (waits only if the GPU still reads it) --------
    FrameSlot& slot = m.ring.acquire(m.device, cmd);

    prepare_instances(m.device, scene, camera.position, m.meshes, m.materials, slot, m.frame_instances, false);
    upload_scene_buffers(m.device, scene, slot, m.frame_instances, false);

    bool shadow_valid = false;
    float texel_world = 0.0f;
    const Mat4 light_vp = light_view_projection(scene, [&](MeshHandle mesh) { return m.meshes.get(mesh); },
                                                m.settings.shadow_map_size, shadow_valid, texel_world);
    const bool shadows = scene.sun.shadows && shadow_valid && !scene.meshes.empty();
    const Mat4 view_projection = camera.projection * camera.view;
    const Vec3 sun = normalize(scene.sun.direction);
    const auto counts_sdf = static_cast<std::uint32_t>(m.frame_instances.shapes.size());
    GpuFrame frame{view_projection,
                   inverse(view_projection),
                   light_vp,
                   {camera.position.x, camera.position.y, camera.position.z, 1},
                   {sun.x, sun.y, sun.z, texel_world * 1.5f},
                   {scene.sun.color.x * scene.sun.intensity, scene.sun.color.y * scene.sun.intensity,
                    scene.sun.color.z * scene.sun.intensity, 0},
                   {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z, scene.environment.ambient},
                   {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z, scene.environment.exposure},
                   {static_cast<std::uint32_t>(m.frame_instances.lights.size()), counts_sdf,
                    shadows ? m.device.bindless_index(m.shadow_map) : no_shadow, 0},
                   {static_cast<float>(w), static_cast<float>(h), 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h)},
                   Mat4::identity(), {}, {}, {}, {}, {}, view_projection, {}};
    {
        const std::uint64_t params = m.device.buffer_address(slot.params.buffer); // written by prepare_instances
        frame.custom[0] = static_cast<std::uint32_t>(params);
        frame.custom[1] = static_cast<std::uint32_t>(params >> 32);
        std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
        frame.custom[3] = is_srgb(m.target_format) ? 0u : 1u; // UNORM targets need shaders to encode
    }
    slot.frame.write(m.device, &frame, sizeof(frame));

    const std::uint64_t instance_address = m.device.buffer_address(slot.instances.buffer);
    const std::uint64_t material_address = m.device.buffer_address(slot.materials.buffer);
    const std::uint64_t frame_address = m.device.buffer_address(slot.frame.buffer);
    const std::uint64_t light_address = m.device.buffer_address(slot.lights.buffer);
    const std::uint64_t shape_address = m.device.buffer_address(slot.shapes.buffer);
    const std::uint64_t params_address = m.device.buffer_address(slot.params.buffer);

    // Draws every mesh run (one instanced call per mesh and shader), binding each shader's pipeline.
    auto draw_meshes = [&, instance_address, frame_address](gpu::CommandList& c, bool gbuffer) {
        const auto& order = m.frame_instances.order;
        const auto& instance_shader = m.frame_instances.instance_shader;
        std::uint32_t start = 0;
        std::uint32_t bound = 0xFFFFFFFFu;
        while (start < order.size()) {
            const MeshHandle handle = scene.meshes[order[start]].mesh;
            const std::uint32_t key = instance_shader[start];
            std::uint32_t end = start + 1;
            while (end < order.size() && scene.meshes[order[end]].mesh == handle && instance_shader[end] == key) ++end;
            if (key != bound) {
                gpu::PipelineHandle pipeline = gbuffer ? m.gbuffer : m.shadow;
                if (key != 0) {
                    const GpuShader* shader = m.shaders.find_by_key(key);
                    if (shader && !gbuffer && !shader->casts_shadows) {
                        start = end;
                        continue;
                    }
                    if (shader) pipeline = gbuffer ? shader->gbuffer : shader->shadow;
                }
                c.bind_pipeline(pipeline);
                bound = key;
            }
            if (const GpuMesh* mesh = m.meshes.get(handle)) {
                const MeshPush push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, material_address, start,
                                    params_address, scene.time, light_address};
                c.push_constants(&push, sizeof(push));
                c.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
                c.draw_indexed(mesh->index_count, end - start);
                if (gbuffer) {
                    ++m.stats.draw_calls;
                    m.stats.instances += end - start;
                } else {
                    ++m.stats.shadow_draw_calls;
                }
            }
            start = end;
        }
    };

    // --- Graph -------------------------------------------------------------------------
    RenderGraph& g = m.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    const RGTexture shadow_map = g.import(m.shadow_map, "shadow_map");
    const RGTexture albedo = g.create("gbuffer.albedo", {w, h, gpu::Format::RGBA8Unorm});
    const RGTexture normal = g.create("gbuffer.normal", {w, h, gpu::Format::RGBA16Float});
    const RGTexture emissive = g.create("gbuffer.emissive", {w, h, gpu::Format::RGBA16Float});
    const RGTexture depth = g.create("gbuffer.depth", {w, h, gpu::Format::D32Float});
    const RGTexture velocity = g.create("gbuffer.velocity", {w, h, gpu::Format::RG16Float});
    const RGTexture hdr = g.create("hdr", {w, h, gpu::Format::RGBA16Float});
    RGTexture sdf_albedo, sdf_normal, sdf_depth, sdf_velocity;

    if (shadows) {
        g.add_pass("shadow", [&](PassBuilder& b) {
            b.depth(shadow_map, gpu::LoadOp::Clear, 1.0f);
            b.side_effect(); // persistent texture read through the frame data, not the graph
        }, [&](PassContext& ctx) {
            draw_meshes(ctx.cmd, false);
        });
    }
    g.add_pass("gbuffer", [&](PassBuilder& b) {
        b.color(albedo, gpu::LoadOp::Clear, {0, 0, 0, 1});
        b.color(normal, gpu::LoadOp::Clear, {0, 1, 0, 0});
        b.color(emissive, gpu::LoadOp::Clear, {0, 0, 0, 0});
        b.color(velocity, gpu::LoadOp::Clear, {0, 0, 0, 0});
        b.depth(depth, gpu::LoadOp::Clear, 1.0f);
    }, [&](PassContext& ctx) {
        draw_meshes(ctx.cmd, true);
    });
    if (counts_sdf > 0) {
        sdf_albedo = g.create("sdf.albedo", {w, h, gpu::Format::RGBA8Unorm});
        sdf_normal = g.create("sdf.normal", {w, h, gpu::Format::RGBA16Float});
        sdf_depth = g.create("sdf.depth", {w, h, gpu::Format::R32Float});
        sdf_velocity = g.create("sdf.velocity", {w, h, gpu::Format::RG16Float});
        g.add_pass("sdf", [&](PassBuilder& b) {
            b.read(depth);
            b.write_storage(sdf_albedo);
            b.write_storage(sdf_normal);
            b.write_storage(sdf_depth);
            b.write_storage(sdf_velocity);
        }, [&](PassContext& ctx) {
            const SdfPush push{frame_address, shape_address, ctx.bindless(depth), ctx.bindless(sdf_albedo),
                               ctx.bindless(sdf_normal), ctx.bindless(sdf_depth), ctx.bindless(sdf_velocity)};
            ctx.cmd.bind_pipeline(m.sdf);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    }
    g.add_pass("lighting", [&](PassBuilder& b) {
        for (RGTexture t : {albedo, normal, emissive, depth}) b.read(t);
        if (counts_sdf > 0) {
            for (RGTexture t : {sdf_albedo, sdf_normal, sdf_depth}) b.read(t);
        }
        if (shadows) b.read(shadow_map);
        b.write_storage(hdr);
    }, [&](PassContext& ctx) {
        const std::uint32_t none = ctx.bindless(depth); // any valid index for unused slots
        const LightingPush push{frame_address, light_address, shape_address,
                                ctx.bindless(albedo), ctx.bindless(normal), ctx.bindless(emissive), ctx.bindless(depth),
                                counts_sdf ? ctx.bindless(sdf_albedo) : none,
                                counts_sdf ? ctx.bindless(sdf_normal) : none,
                                counts_sdf ? ctx.bindless(sdf_depth) : none, ctx.bindless(hdr)};
        ctx.cmd.bind_pipeline(m.lighting);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    m.add_sky(g, hdr, depth, frame_address, material_address, scene.sky);
    std::vector<MaterialHandle> post_chain;
    for (MaterialHandle p : scene.post) {
        if (m.materials.is_post(p)) post_chain.push_back(p);
    }
    const RGTexture scene_color = post_chain.empty() ? output : g.create("scene.ldr", {w, h, m.target_format});
    g.add_pass("tonemap", [&](PassBuilder& b) {
        b.read(hdr);
        b.color(scene_color, gpu::LoadOp::DontCare);
    }, [&](PassContext& ctx) {
        const TonemapPush push{ctx.bindless(hdr), scene.environment.exposure, is_srgb(m.target_format) ? 0u : 1u};
        ctx.cmd.bind_pipeline(m.tonemap);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.draw(3);
    });
    m.add_transparent(g, scene_color, depth, instance_address, frame_address, material_address, params_address, light_address, scene.time);
    m.add_post(g, scene_color, output, post_chain, w, h, frame_address, material_address);
    m.add_debug_lines(g, slot, output, depth, camera.projection * camera.view, scene.debug_lines);
    m.add_ui(g, slot, output, w, h, scene.ui);
    g.execute(cmd);

    m.stats.sdf_shapes = counts_sdf;
    m.stats.point_lights = static_cast<std::uint32_t>(m.frame_instances.lights.size());
    m.stats.passes = g.stats().passes_executed;
}

// =========================================================================================
// Ray-traced path (ADR-0011)
// =========================================================================================

void Renderer::Impl::ensure_history(std::uint32_t w, std::uint32_t h) {
    if (history.width == w && history.height == h) return;
    history.destroy(device);
    history.width = w;
    history.height = h;
    auto make = [&](const char* name) {
        return device.create_texture({.width = w, .height = h, .format = gpu::Format::RGBA16Float,
                                      .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                      .debug_name = name});
    };
    for (int i = 0; i < 2; ++i) {
        history.geometry[i] = make("astral.history.geometry");
        history.diffuse[i] = make("astral.history.diffuse");
        history.specular[i] = make("astral.history.specular");
        history.taa[i] = make("astral.history.taa");
        history.moments[i] = make("astral.history.moments");
    }
    history_valid = false;
}

void Renderer::Impl::ensure_ddgi(const RenderScene& scene) {
    const GiSettings& gi = settings.gi;
    const std::uint32_t nx = std::max(gi.probes_x, 2u), ny = std::max(gi.probes_y, 2u), nz = std::max(gi.probes_z, 2u);
    const std::uint32_t rays = std::clamp(gi.rays_per_probe, 16u, 512u);

    // Volume: from the settings, or fitted around every mesh's world bounds.
    Vec3 lo = gi.volume_min, hi = gi.volume_max;
    if (lo.x == hi.x && lo.y == hi.y && lo.z == hi.z) {
        lo = {1e30f, 1e30f, 1e30f};
        hi = {-1e30f, -1e30f, -1e30f};
        for (const MeshInstance& d : scene.meshes) {
            const GpuMesh* mesh = meshes.get(d.mesh);
            if (!mesh) continue;
            for (int i = 0; i < 8; ++i) {
                const Vec3 c{(i & 1) ? mesh->bounds_max.x : mesh->bounds_min.x, (i & 2) ? mesh->bounds_max.y : mesh->bounds_min.y,
                             (i & 4) ? mesh->bounds_max.z : mesh->bounds_min.z};
                const Vec4 p = d.transform * Vec4{c.x, c.y, c.z, 1};
                lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            }
        }
        for (const SdfShape& sh : scene.sdf) {
            const float r = std::max({sh.size.x, sh.size.y, sh.size.z}) + sh.blend;
            lo = {std::min(lo.x, sh.position.x - r), std::min(lo.y, sh.position.y - r), std::min(lo.z, sh.position.z - r)};
            hi = {std::max(hi.x, sh.position.x + r), std::max(hi.y, sh.position.y + r), std::max(hi.z, sh.position.z + r)};
        }
        if (lo.x > hi.x) lo = hi = Vec3{};
    }
    const Vec3 extent{std::max(hi.x - lo.x, 0.5f), std::max(hi.y - lo.y, 0.5f), std::max(hi.z - lo.z, 0.5f)};
    // Probes sit at cell centres so none lies exactly on a wall.
    const Vec3 spacing{extent.x / static_cast<float>(nx), extent.y / static_cast<float>(ny), extent.z / static_cast<float>(nz)};
    const Vec3 origin = lo + spacing * 0.5f;

    const bool same_grid = ddgi.nx == nx && ddgi.ny == ny && ddgi.nz == nz && ddgi.rays == rays;
    const float moved = length(origin - ddgi.origin) + length(spacing - ddgi.spacing);
    if (same_grid) {
        if (moved > 0.01f * length(extent)) ddgi.valid = false; // the volume moved: start over
        ddgi.origin = origin;
        ddgi.spacing = spacing;
        return;
    }
    ddgi.destroy(device);
    ddgi.nx = nx;
    ddgi.ny = ny;
    ddgi.nz = nz;
    ddgi.rays = rays;
    ddgi.origin = origin;
    ddgi.spacing = spacing;
    auto atlas = [&](int interior, const char* name) {
        return device.create_texture({.width = nx * ny * static_cast<std::uint32_t>(interior + 2),
                                      .height = nz * static_cast<std::uint32_t>(interior + 2),
                                      .format = gpu::Format::RGBA16Float,
                                      .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                      .debug_name = name});
    };
    for (int i = 0; i < 2; ++i) {
        ddgi.irradiance[i] = atlas(8, "astral.ddgi.irradiance");
        ddgi.distance[i] = atlas(16, "astral.ddgi.distance");
    }
    ddgi.ray_buffer = device.create_buffer({.size = std::uint64_t{nx} * ny * nz * rays * 16, .debug_name = "astral.ddgi.rays"});
    ddgi.valid = false;
}

void Renderer::Impl::render_ray_traced(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                                       const RenderScene& scene, std::uint32_t w, std::uint32_t h) {
    auto& m = *this;
    m.shaders.rebuild_rt_surfaces_if_dirty();
    const RtPipelines& rt_pipes = m.shaders.rt_pipelines();
    FrameSlot& slot = m.ring.acquire(m.device, cmd);

    ensure_history(w, h);
    ensure_ddgi(scene);

    // --- Scene data ------------------------------------------------------------------------
    prepare_instances(m.device, scene, camera.position, m.meshes, m.materials, slot, m.frame_instances, true);
    upload_scene_buffers(m.device, scene, slot, m.frame_instances, true);

    if (!build_tlas(m.device, cmd, m.frame_instances)) return;

    // --- Frame constants: jittered projection for TAA -------------------------------------
    const std::uint32_t jitter_index = (m.frame_index % 16) + 1;
    const float jx = (halton(jitter_index, 2) - 0.5f), jy = (halton(jitter_index, 3) - 0.5f);
    const Mat4 unjittered = camera.projection * camera.view;
    const Mat4 jittered = translate({2.0f * jx / static_cast<float>(w), 2.0f * jy / static_cast<float>(h), 0}) * unjittered;
    const Vec3 sun = normalize(scene.sun.direction);
    const auto sdf_count = static_cast<std::uint32_t>(m.frame_instances.shapes.size());
    const std::uint32_t parity = m.frame_index & 1u, prev = parity ^ 1u;
    GpuFrame frame{};
    frame.view_projection = jittered;
    frame.inv_view_projection = inverse(jittered);
    frame.light_view_projection = Mat4::identity();
    frame.camera_position = {camera.position.x, camera.position.y, camera.position.z, 1};
    frame.sun_direction = {sun.x, sun.y, sun.z, 0};
    const float sun_on = scene.sun.intensity;
    frame.sun_color = {scene.sun.color.x * sun_on, scene.sun.color.y * sun_on, scene.sun.color.z * sun_on, 0};
    frame.sky_color = {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z, scene.environment.ambient};
    frame.ground_color = {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z, scene.environment.exposure};
    frame.counts[0] = static_cast<std::uint32_t>(m.frame_instances.lights.size());
    frame.counts[1] = sdf_count;
    frame.counts[2] = no_shadow;
    frame.screen = {static_cast<float>(w), static_cast<float>(h), 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h)};
    frame.prev_view_projection = m.history_valid ? m.prev_view_projection : unjittered;
    frame.ddgi_origin = {ddgi.origin.x, ddgi.origin.y, ddgi.origin.z, ddgi.valid ? settings.gi.hysteresis : 0.0f};
    frame.ddgi_spacing = {ddgi.spacing.x, ddgi.spacing.y, ddgi.spacing.z, 0};
    frame.ddgi_counts[0] = ddgi.nx;
    frame.ddgi_counts[1] = ddgi.ny;
    frame.ddgi_counts[2] = ddgi.nz;
    frame.ddgi_counts[3] = ddgi.rays;
    frame.rt_counts[0] = static_cast<std::uint32_t>(m.frame_instances.rects.size());
    frame.rt_counts[1] = m.frame_index;
    frame.rt_counts[2] = m.history_valid ? 1u : 0u;
    frame.rt_counts[3] = ddgi.valid ? 1u : 0u;
    frame.rt_params = {std::max(scene.sun.angular_radius, 0.0f), jx, jy, settings.max_ray_distance};
    frame.unjittered_view_projection = unjittered;
    {
        const std::uint64_t params = m.device.buffer_address(slot.params.buffer); // written by prepare_instances
        frame.custom[0] = static_cast<std::uint32_t>(params);
        frame.custom[1] = static_cast<std::uint32_t>(params >> 32);
        std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
        frame.custom[3] = is_srgb(m.target_format) ? 0u : 1u;
    }
    slot.frame.write(m.device, &frame, sizeof(frame));

    const std::uint64_t frame_address = m.device.buffer_address(slot.frame.buffer);
    const std::uint64_t instance_address = m.device.buffer_address(slot.instances.buffer);
    const std::uint64_t material_address = m.device.buffer_address(slot.materials.buffer);
    const std::uint64_t light_address = m.device.buffer_address(slot.lights.buffer);
    const std::uint64_t shape_address = m.device.buffer_address(slot.shapes.buffer);
    const std::uint64_t rect_address = m.device.buffer_address(slot.rects.buffer);
    const std::uint64_t tlas_address = m.device.acceleration_structure_address(m.frame_instances.tlas);
    const std::uint64_t rays_address = m.device.buffer_address(ddgi.ray_buffer);

    // --- Graph -------------------------------------------------------------------------------
    RenderGraph& g = m.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    const RGTexture albedo = g.create("gbuffer.albedo", {w, h, gpu::Format::RGBA8Unorm});
    const RGTexture normal = g.create("gbuffer.normal", {w, h, gpu::Format::RGBA16Float});
    const RGTexture emissive = g.create("gbuffer.emissive", {w, h, gpu::Format::RGBA16Float});
    const RGTexture depth = g.create("gbuffer.depth", {w, h, gpu::Format::D32Float});
    const RGTexture velocity = g.create("gbuffer.velocity", {w, h, gpu::Format::RG16Float});
    RGTexture sdf_albedo = albedo, sdf_normal = normal, sdf_depth = depth, sdf_velocity = velocity;
    const RGTexture irr_prev = g.import(ddgi.irradiance[prev], "ddgi.irradiance.prev");
    const RGTexture irr = g.import(ddgi.irradiance[parity], "ddgi.irradiance");
    const RGTexture dist_prev = g.import(ddgi.distance[prev], "ddgi.distance.prev");
    const RGTexture dist = g.import(ddgi.distance[parity], "ddgi.distance");
    const RGTexture geometry = g.import(history.geometry[parity], "history.geometry");
    const RGTexture geometry_prev = g.import(history.geometry[prev], "history.geometry.prev");
    const RGTexture hist_d = g.import(history.diffuse[parity], "history.diffuse");
    const RGTexture hist_d_prev = g.import(history.diffuse[prev], "history.diffuse.prev");
    const RGTexture hist_s = g.import(history.specular[parity], "history.specular");
    const RGTexture hist_s_prev = g.import(history.specular[prev], "history.specular.prev");
    const RGTexture moments = g.import(history.moments[parity], "history.moments");
    const RGTexture moments_prev = g.import(history.moments[prev], "history.moments.prev");
    const RGTexture taa_out = g.import(history.taa[parity], "history.taa");
    const RGTexture taa_prev = g.import(history.taa[prev], "history.taa.prev");
    const RGTexture noisy_d = g.create("rt.diffuse", {w, h, gpu::Format::RGBA16Float});
    const RGTexture noisy_s = g.create("rt.specular", {w, h, gpu::Format::RGBA16Float});
    const RGTexture hdr = g.create("hdr", {w, h, gpu::Format::RGBA16Float});

    const std::uint64_t params_address = m.device.buffer_address(slot.params.buffer);
    auto draw_meshes = [&](gpu::CommandList& c) {
        const auto& order = m.frame_instances.order;
        const auto& instance_shader = m.frame_instances.instance_shader;
        std::uint32_t start = 0;
        std::uint32_t bound = 0xFFFFFFFFu;
        while (start < order.size()) {
            const MeshHandle handle = scene.meshes[order[start]].mesh;
            const std::uint32_t key = instance_shader[start];
            std::uint32_t end = start + 1;
            while (end < order.size() && scene.meshes[order[end]].mesh == handle && instance_shader[end] == key) ++end;
            if (key != bound) {
                const GpuShader* shader = m.shaders.find_by_key(key);
                c.bind_pipeline(shader ? shader->gbuffer : m.gbuffer);
                bound = key;
            }
            const GpuMesh* mesh = m.meshes.get(handle);
            const MeshPush push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, material_address, start,
                                params_address, scene.time, light_address};
            c.push_constants(&push, sizeof(push));
            c.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
            c.draw_indexed(mesh->index_count, end - start);
            ++m.stats.draw_calls;
            m.stats.instances += end - start;
            start = end;
        }
    };
    g.add_pass("gbuffer", [&](PassBuilder& b) {
        b.color(albedo, gpu::LoadOp::Clear, {0, 0, 0, 1});
        b.color(normal, gpu::LoadOp::Clear, {0, 1, 0, 0});
        b.color(emissive, gpu::LoadOp::Clear, {0, 0, 0, 0});
        b.color(velocity, gpu::LoadOp::Clear, {0, 0, 0, 0});
        b.depth(depth, gpu::LoadOp::Clear, 1.0f);
    }, [&](PassContext& ctx) {
        draw_meshes(ctx.cmd);
    });
    if (sdf_count > 0) {
        sdf_albedo = g.create("sdf.albedo", {w, h, gpu::Format::RGBA8Unorm});
        sdf_normal = g.create("sdf.normal", {w, h, gpu::Format::RGBA16Float});
        sdf_depth = g.create("sdf.depth", {w, h, gpu::Format::R32Float});
        sdf_velocity = g.create("sdf.velocity", {w, h, gpu::Format::RG16Float});
        g.add_pass("sdf", [&](PassBuilder& b) {
            b.read(depth);
            b.write_storage(sdf_albedo);
            b.write_storage(sdf_normal);
            b.write_storage(sdf_depth);
            b.write_storage(sdf_velocity);
        }, [&](PassContext& ctx) {
            const SdfPush push{frame_address, shape_address, ctx.bindless(depth), ctx.bindless(sdf_albedo),
                               ctx.bindless(sdf_normal), ctx.bindless(sdf_depth), ctx.bindless(sdf_velocity)};
            ctx.cmd.bind_pipeline(m.sdf);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    }
    const bool has_sdf = sdf_count > 0;
    auto gbuffer_reads = [&](PassBuilder& b) {
        for (RGTexture t : {albedo, normal, emissive, depth, velocity}) b.read(t);
        if (has_sdf) {
            for (RGTexture t : {sdf_albedo, sdf_normal, sdf_depth, sdf_velocity}) b.read(t);
        }
    };
    auto gbuffer_ids = [&](PassContext& ctx) {
        return GBufferIds{ctx.bindless(albedo), ctx.bindless(normal), ctx.bindless(emissive), ctx.bindless(depth),
                          ctx.bindless(velocity), ctx.bindless(sdf_albedo), ctx.bindless(sdf_normal),
                          ctx.bindless(sdf_depth), ctx.bindless(sdf_velocity)};
    };

    // DDGI: trace probe rays (lit with last frame's probes), then blend into the atlases.
    const std::uint32_t probe_rays = ddgi.nx * ddgi.ny * ddgi.nz * ddgi.rays;
    g.add_pass("ddgi.trace", [&](PassBuilder& b) {
        b.read(irr_prev);
        b.read(dist_prev);
        b.side_effect(); // writes the probe ray buffer
    }, [&](PassContext& ctx) {
        const DdgiTracePush push{frame_address, tlas_address, instance_address, material_address, light_address, rect_address,
                                 shape_address, rays_address, ctx.bindless(irr_prev), ctx.bindless(dist_prev)};
        ctx.cmd.bind_pipeline(rt_pipes.ddgi_trace);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((probe_rays + 63) / 64);
    });
    for (std::uint32_t mode = 0; mode < 2; ++mode) {
        const RGTexture src = mode == 0 ? irr_prev : dist_prev;
        const RGTexture dst = mode == 0 ? irr : dist;
        const gpu::TextureDesc desc = m.device.texture_desc(mode == 0 ? ddgi.irradiance[0] : ddgi.distance[0]);
        g.add_pass(mode == 0 ? "ddgi.update.irradiance" : "ddgi.update.distance", [&](PassBuilder& b) {
            b.read(src);
            b.write_storage(dst);
        }, [&, src, dst, mode, desc](PassContext& ctx) {
            ctx.cmd.memory_barrier(); // probe ray buffer written by ddgi.trace
            const DdgiUpdatePush push{frame_address, rays_address, ctx.bindless(src), ctx.bindless(dst), mode};
            ctx.cmd.bind_pipeline(rt_pipes.ddgi_update);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((desc.width + 7) / 8, (desc.height + 7) / 8);
        });
    }

    g.add_pass("rt.lighting", [&](PassBuilder& b) {
        gbuffer_reads(b);
        b.read(irr);
        b.read(dist);
        b.write_storage(noisy_d);
        b.write_storage(noisy_s);
        b.write_storage(geometry);
    }, [&](PassContext& ctx) {
        const RtLightingPush push{frame_address, tlas_address, instance_address, material_address, light_address, rect_address, shape_address,
                                  gbuffer_ids(ctx), ctx.bindless(irr), ctx.bindless(dist),
                                  ctx.bindless(noisy_d), ctx.bindless(noisy_s), ctx.bindless(geometry),
                                  std::max(settings.near_gi_radius, 0.0f)};
        ctx.cmd.bind_pipeline(rt_pipes.rt_lighting);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    g.add_pass("denoise.temporal", [&](PassBuilder& b) {
        gbuffer_reads(b);
        for (RGTexture t : {geometry, geometry_prev, noisy_d, noisy_s, hist_d_prev, hist_s_prev, moments_prev}) b.read(t);
        b.write_storage(hist_d);
        b.write_storage(hist_s);
        b.write_storage(moments);
    }, [&](PassContext& ctx) {
        const TemporalPush push{frame_address, gbuffer_ids(ctx), ctx.bindless(geometry), ctx.bindless(geometry_prev),
                                ctx.bindless(noisy_d), ctx.bindless(noisy_s), ctx.bindless(hist_d_prev),
                                ctx.bindless(hist_s_prev), ctx.bindless(hist_d), ctx.bindless(hist_s),
                                std::max(settings.max_history, 1.0f), ctx.bindless(moments_prev), ctx.bindless(moments)};
        ctx.cmd.bind_pipeline(rt_pipes.temporal);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    RGTexture final_d = hist_d, final_s = hist_s;
    for (std::uint32_t i = 0; i < settings.denoise_iterations; ++i) {
        const RGTexture in_d = final_d, in_s = final_s;
        const RGTexture out_d = g.create("denoise.diffuse", {w, h, gpu::Format::RGBA16Float});
        const RGTexture out_s = g.create("denoise.specular", {w, h, gpu::Format::RGBA16Float});
        const std::int32_t step = 1 << i;
        g.add_pass("denoise.atrous", [&, in_d, in_s, out_d, out_s](PassBuilder& b) {
            gbuffer_reads(b);
            b.read(geometry);
            b.read(moments);
            b.read(in_d);
            b.read(in_s);
            b.write_storage(out_d);
            b.write_storage(out_s);
        }, [&, in_d, in_s, out_d, out_s, step](PassContext& ctx) {
            const AtrousPush push{frame_address, gbuffer_ids(ctx), ctx.bindless(geometry), ctx.bindless(in_d),
                                  ctx.bindless(in_s), ctx.bindless(out_d), ctx.bindless(out_s), step,
                                  ctx.bindless(moments)};
            ctx.cmd.bind_pipeline(rt_pipes.atrous);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
        final_d = out_d;
        final_s = out_s;
    }
    g.add_pass("composite", [&](PassBuilder& b) {
        gbuffer_reads(b);
        for (RGTexture t : {final_d, final_s}) b.read(t);
        b.write_storage(hdr);
    }, [&](PassContext& ctx) {
        const CompositePush push{frame_address, rect_address, gbuffer_ids(ctx), ctx.bindless(final_d),
                                 ctx.bindless(final_s), ctx.bindless(hdr), static_cast<std::uint32_t>(settings.debug_view)};
        ctx.cmd.bind_pipeline(rt_pipes.composite);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    m.add_sky(g, hdr, depth, frame_address, material_address, scene.sky);
    g.add_pass("taa", [&](PassBuilder& b) {
        gbuffer_reads(b);
        b.read(hdr);
        b.read(taa_prev);
        b.write_storage(taa_out);
    }, [&](PassContext& ctx) {
        const TaaPush push{frame_address, gbuffer_ids(ctx), ctx.bindless(hdr), ctx.bindless(taa_prev),
                           ctx.bindless(taa_out), std::clamp(settings.taa_blend, 0.01f, 1.0f)};
        ctx.cmd.bind_pipeline(rt_pipes.taa);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });

    // Bloom: half-resolution mip chain down, then tent-filtered back up.
    constexpr std::uint32_t bloom_levels = 6;
    std::array<RGTexture, bloom_levels> down{}, up{};
    std::array<gpu::Extent2D, bloom_levels> sizes{};
    std::uint32_t levels = 0;
    const bool bloom = settings.bloom_strength > 0.0f;
    for (std::uint32_t i = 0; bloom && i < bloom_levels; ++i) {
        const std::uint32_t lw = std::max(1u, w >> (i + 1)), lh = std::max(1u, h >> (i + 1));
        sizes[i] = {lw, lh};
        down[i] = g.create("bloom.down", {lw, lh, gpu::Format::RGBA16Float});
        const RGTexture src = i == 0 ? taa_out : down[i - 1];
        const gpu::Extent2D src_size = i == 0 ? gpu::Extent2D{w, h} : sizes[i - 1];
        const RGTexture dst = down[i];
        g.add_pass("bloom.down", [&, src, dst](PassBuilder& b) {
            b.read(src);
            b.write_storage(dst);
        }, [&, src, dst, src_size, lw, lh, i](PassContext& ctx) {
            const BloomDownPush push{ctx.bindless(src), ctx.bindless(dst),
                                     {1.0f / static_cast<float>(src_size.width), 1.0f / static_cast<float>(src_size.height)},
                                     {lw, lh}, i == 0 ? 1u : 0u};
            ctx.cmd.bind_pipeline(rt_pipes.bloom_down);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((lw + 7) / 8, (lh + 7) / 8);
        });
        ++levels;
        if (lw == 1 && lh == 1) break;
    }
    RGTexture bloom_result{};
    if (levels > 0) {
        RGTexture low = down[levels - 1];
        for (int i = static_cast<int>(levels) - 2; i >= 0; --i) {
            const auto li = static_cast<std::uint32_t>(i);
            up[li] = g.create("bloom.up", {sizes[li].width, sizes[li].height, gpu::Format::RGBA16Float});
            const RGTexture current = down[li], dst = up[li], lower = low;
            const gpu::Extent2D low_size = sizes[li + 1], size = sizes[li];
            g.add_pass("bloom.up", [&, lower, current, dst](PassBuilder& b) {
                b.read(lower);
                b.read(current);
                b.write_storage(dst);
            }, [&, lower, current, dst, low_size, size](PassContext& ctx) {
                const BloomUpPush push{ctx.bindless(lower), ctx.bindless(current), ctx.bindless(dst),
                                       {1.0f / static_cast<float>(low_size.width), 1.0f / static_cast<float>(low_size.height)},
                                       {size.width, size.height}, 1.0f};
                ctx.cmd.bind_pipeline(rt_pipes.bloom_up);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((size.width + 7) / 8, (size.height + 7) / 8);
            });
            low = dst;
        }
        bloom_result = low;
    }
    std::vector<MaterialHandle> post_chain;
    for (MaterialHandle p : scene.post) {
        if (m.materials.is_post(p)) post_chain.push_back(p);
    }
    const RGTexture scene_color = post_chain.empty() ? output : g.create("scene.ldr", {w, h, m.target_format});
    g.add_pass("tonemap", [&](PassBuilder& b) {
        b.read(taa_out);
        if (bloom_result.valid()) b.read(bloom_result);
        b.color(scene_color, gpu::LoadOp::DontCare);
    }, [&](PassContext& ctx) {
        TonemapPush push{ctx.bindless(taa_out), scene.environment.exposure, is_srgb(m.target_format) ? 0u : 1u};
        if (bloom_result.valid()) {
            push.bloom = ctx.bindless(bloom_result);
            push.bloom_strength = settings.bloom_strength;
        }
        ctx.cmd.bind_pipeline(m.tonemap);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.draw(3);
    });
    m.add_transparent(g, scene_color, depth, instance_address, frame_address, material_address, params_address, light_address, scene.time);
    m.add_post(g, scene_color, output, post_chain, w, h, frame_address, material_address);
    m.add_debug_lines(g, slot, output, depth, unjittered, scene.debug_lines);
    m.add_ui(g, slot, output, w, h, scene.ui);
    g.execute(cmd);

    m.prev_view_projection = unjittered;
    m.history_valid = true;
    ddgi.valid = true;
    ++m.frame_index;
    m.stats.ray_traced = true;
    m.stats.sdf_shapes = sdf_count;
    m.stats.point_lights = static_cast<std::uint32_t>(m.frame_instances.lights.size());
    m.stats.rect_lights = static_cast<std::uint32_t>(m.frame_instances.rects.size());
    m.stats.gi_probes = ddgi.nx * ddgi.ny * ddgi.nz;
    m.stats.passes = g.stats().passes_executed;
}

} // namespace astral
