#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <shaders/gbuffer.frag.hpp>
#include <shaders/gbuffer.vert.hpp>
#include <shaders/lighting.comp.hpp>
#include <shaders/sdf.comp.hpp>
#include <shaders/shadow.vert.hpp>
#include <shaders/tonemap.frag.hpp>
#include <shaders/tonemap.vert.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace astral {

namespace {

// --- GPU layouts (scalar; mirror shaders/include/astral/scene.glsl) ----------------------
struct GpuMesh {
    gpu::BufferHandle vertices;
    gpu::BufferHandle indices;
    std::uint32_t index_count = 0;
    Vec3 bounds_min{}, bounds_max{}; // local-space AABB
};
struct GpuInstance {
    Mat4 model;
    Vec4 albedo;
    Vec4 material;
};
struct GpuFrame {
    Mat4 view_projection;
    Mat4 inv_view_projection;
    Mat4 light_view_projection;
    Vec4 camera_position;
    Vec4 sun_direction;
    Vec4 sun_color;
    Vec4 sky_color;
    Vec4 ground_color;
    std::uint32_t counts[4];
    Vec4 screen;
};
struct GpuPointLight {
    Vec4 position_radius;
    Vec4 color_intensity;
};
struct GpuSdfShape {
    Vec4 position_blend;
    Vec4 rotation;
    Vec4 size;
    Vec4 albedo_roughness;
    Vec4 params;
};
struct MeshPush {
    std::uint64_t vertices, instances, frame;
    std::uint32_t first_instance;
};
struct SdfPush {
    std::uint64_t frame, shapes;
    std::uint32_t mesh_depth, out_albedo, out_normal, out_depth;
};
struct LightingPush {
    std::uint64_t frame, lights, shapes;
    std::uint32_t albedo, normal, emissive, depth, sdf_albedo, sdf_normal, sdf_depth, out_hdr;
};
struct TonemapPush {
    std::uint32_t hdr;
    float exposure;
    std::uint32_t encode_srgb;
};
static_assert(sizeof(LightingPush) <= gpu::max_push_constant_size);

constexpr std::uint32_t ring_size = 3;
constexpr std::uint32_t no_shadow = 0xFFFFFFFFu;

/// A host-visible buffer that grows on demand (old buffers are destroyed deferred).
struct GrowBuffer {
    gpu::BufferHandle buffer;
    std::uint64_t capacity = 0;
    void write(gpu::Device& device, const void* data, std::uint64_t bytes) {
        const std::uint64_t needed = std::max<std::uint64_t>(bytes, 64);
        if (capacity < needed) {
            device.destroy(buffer);
            capacity = std::max(needed, capacity * 2);
            buffer = device.create_buffer({.size = capacity, .usage = gpu::BufferUsage::Storage,
                                           .memory = gpu::MemoryUsage::CpuToGpu});
        }
        if (bytes) device.write_buffer(buffer, data, bytes);
    }
};

struct FrameSlot {
    GrowBuffer instances, lights, shapes, frame;
    std::shared_ptr<std::uint64_t> last_use = std::make_shared<std::uint64_t>(0);
};

bool is_srgb(gpu::Format f) { return f == gpu::Format::RGBA8Srgb || f == gpu::Format::BGRA8Srgb; }

/// Orthographic sun view fitted tightly (in light space) around every mesh's transformed
/// bounding box and every SDF shape. Also returns the world size of one shadow texel.
template <class MeshLookup>
Mat4 light_view_projection(const RenderScene& scene, MeshLookup&& mesh_of, std::uint32_t map_size,
                           bool& valid, float& texel_world) {
    std::vector<Vec3> points;
    for (const DrawItem& d : scene.meshes) {
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

} // namespace

struct Renderer::Impl {
    gpu::Device& device;
    gpu::Format target_format;
    RenderSettings settings;
    RenderGraph graph;
    HandlePool<GpuMesh, MeshTag> meshes;
    gpu::PipelineHandle gbuffer, shadow, sdf, lighting, tonemap;
    gpu::TextureHandle shadow_map;
    std::array<FrameSlot, ring_size> ring{};
    std::uint32_t ring_index = 0;
    RenderStats stats;
    std::vector<std::uint32_t> order;
    std::vector<GpuInstance> instances;
    std::vector<GpuPointLight> lights;
    std::vector<GpuSdfShape> shapes;

    Impl(gpu::Device& d, gpu::Format f, const RenderSettings& s) : device(d), target_format(f), settings(s), graph(d) {}
};

Renderer::Renderer(gpu::Device& device, gpu::Format target_format, const RenderSettings& settings)
    : impl_(std::make_unique<Impl>(device, target_format, settings)) {
    auto& m = *impl_;
    m.gbuffer = device.create_graphics_pipeline({
        .vertex = {shaders::gbuffer_vert},
        .fragment = {shaders::gbuffer_frag},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float},
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
    m.shadow_map = device.create_texture({.width = settings.shadow_map_size, .height = settings.shadow_map_size,
                                          .format = gpu::Format::D32Float,
                                          .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled,
                                          .debug_name = "astral.shadow_map"});
}

Renderer::~Renderer() {
    auto& m = *impl_;
    m.meshes.for_each([&](MeshHandle, GpuMesh& mesh) {
        m.device.destroy(mesh.vertices);
        m.device.destroy(mesh.indices);
    });
    for (auto& slot : m.ring) {
        for (GrowBuffer* b : {&slot.instances, &slot.lights, &slot.shapes, &slot.frame}) m.device.destroy(b->buffer);
    }
    for (auto p : {m.gbuffer, m.shadow, m.sdf, m.lighting, m.tonemap}) m.device.destroy(p);
    m.device.destroy(m.shadow_map);
}

MeshHandle Renderer::create_mesh(const MeshData& data) {
    auto& m = *impl_;
    if (data.vertices.empty() || data.indices.empty()) return {};
    GpuMesh mesh;
    const std::uint64_t vbytes = data.vertices.size() * sizeof(Vertex);
    const std::uint64_t ibytes = data.indices.size() * sizeof(std::uint32_t);
    mesh.vertices = m.device.create_buffer({.size = vbytes, .usage = gpu::BufferUsage::Storage});
    mesh.indices = m.device.create_buffer({.size = ibytes, .usage = gpu::BufferUsage::Index});
    if (!m.device.write_buffer(mesh.vertices, data.vertices.data(), vbytes) ||
        !m.device.write_buffer(mesh.indices, data.indices.data(), ibytes)) {
        m.device.destroy(mesh.vertices);
        m.device.destroy(mesh.indices);
        return {};
    }
    mesh.index_count = static_cast<std::uint32_t>(data.indices.size());
    mesh.bounds_min = mesh.bounds_max = data.vertices[0].position;
    for (const Vertex& v : data.vertices) {
        mesh.bounds_min = {std::min(mesh.bounds_min.x, v.position.x), std::min(mesh.bounds_min.y, v.position.y),
                           std::min(mesh.bounds_min.z, v.position.z)};
        mesh.bounds_max = {std::max(mesh.bounds_max.x, v.position.x), std::max(mesh.bounds_max.y, v.position.y),
                           std::max(mesh.bounds_max.z, v.position.z)};
    }
    return m.meshes.create(mesh);
}

void Renderer::destroy(MeshHandle handle) {
    auto& m = *impl_;
    if (const GpuMesh* mesh = m.meshes.get(handle)) {
        m.device.destroy(mesh->vertices);
        m.device.destroy(mesh->indices);
        m.meshes.destroy(handle);
    }
}

const RenderStats& Renderer::stats() const noexcept { return impl_->stats; }

void Renderer::render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                      const RenderScene& scene) {
    auto& m = *impl_;
    m.stats = {};
    const gpu::TextureDesc target_desc = m.device.texture_desc(target);
    if (target_desc.width == 0) return;
    const std::uint32_t w = target_desc.width, h = target_desc.height;

    // --- Per-frame data into this ring slot (waits only if the GPU still reads it) --------
    FrameSlot& slot = m.ring[m.ring_index];
    m.ring_index = (m.ring_index + 1) % ring_size;
    m.device.wait({*slot.last_use});
    cmd.on_submitted([use = slot.last_use](gpu::SubmitTicket t) { *use = t.value; });

    m.order.resize(scene.meshes.size());
    for (std::uint32_t i = 0; i < m.order.size(); ++i) m.order[i] = i;
    std::sort(m.order.begin(), m.order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return scene.meshes[a].mesh.index < scene.meshes[b].mesh.index;
    });
    m.instances.clear();
    for (std::uint32_t i : m.order) {
        const DrawItem& d = scene.meshes[i];
        m.instances.push_back({d.transform, d.color, {d.roughness, d.metallic, d.emissive, 0}});
    }
    m.lights.clear();
    for (const PointLight& l : scene.lights) {
        m.lights.push_back({{l.position.x, l.position.y, l.position.z, l.radius},
                            {l.color.x, l.color.y, l.color.z, l.intensity}});
    }
    m.shapes.clear();
    for (const SdfShape& s : scene.sdf) {
        m.shapes.push_back({{s.position.x, s.position.y, s.position.z, s.blend},
                            {s.rotation.x, s.rotation.y, s.rotation.z, s.rotation.w},
                            s.size,
                            {s.color.x, s.color.y, s.color.z, s.roughness},
                            {s.metallic, static_cast<float>(s.operation), static_cast<float>(s.primitive), 0}});
    }
    slot.instances.write(m.device, m.instances.data(), m.instances.size() * sizeof(GpuInstance));
    slot.lights.write(m.device, m.lights.data(), m.lights.size() * sizeof(GpuPointLight));
    slot.shapes.write(m.device, m.shapes.data(), m.shapes.size() * sizeof(GpuSdfShape));

    bool shadow_valid = false;
    float texel_world = 0.0f;
    const Mat4 light_vp = light_view_projection(scene, [&](MeshHandle mesh) { return m.meshes.get(mesh); },
                                                m.settings.shadow_map_size, shadow_valid, texel_world);
    const bool shadows = scene.sun.shadows && shadow_valid && !scene.meshes.empty();
    const Mat4 view_projection = camera.projection * camera.view;
    const Vec3 sun = normalize(scene.sun.direction);
    const auto counts_sdf = static_cast<std::uint32_t>(m.shapes.size());
    GpuFrame frame{view_projection,
                   inverse(view_projection),
                   light_vp,
                   {camera.position.x, camera.position.y, camera.position.z, 1},
                   {sun.x, sun.y, sun.z, texel_world * 1.5f},
                   {scene.sun.color.x * scene.sun.intensity, scene.sun.color.y * scene.sun.intensity,
                    scene.sun.color.z * scene.sun.intensity, 0},
                   {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z, scene.environment.ambient},
                   {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z, scene.environment.exposure},
                   {static_cast<std::uint32_t>(m.lights.size()), counts_sdf,
                    shadows ? m.device.bindless_index(m.shadow_map) : no_shadow, 0},
                   {static_cast<float>(w), static_cast<float>(h), 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h)}};
    slot.frame.write(m.device, &frame, sizeof(frame));

    const std::uint64_t instance_address = m.device.buffer_address(slot.instances.buffer);
    const std::uint64_t frame_address = m.device.buffer_address(slot.frame.buffer);
    const std::uint64_t light_address = m.device.buffer_address(slot.lights.buffer);
    const std::uint64_t shape_address = m.device.buffer_address(slot.shapes.buffer);

    // Draws every mesh run (one instanced call per mesh) with the bound pipeline.
    auto draw_meshes = [&, instance_address, frame_address](gpu::CommandList& c, bool gbuffer) {
        std::uint32_t start = 0;
        while (start < m.order.size()) {
            const MeshHandle handle = scene.meshes[m.order[start]].mesh;
            std::uint32_t end = start + 1;
            while (end < m.order.size() && scene.meshes[m.order[end]].mesh == handle) ++end;
            if (const GpuMesh* mesh = m.meshes.get(handle)) {
                const MeshPush push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, start};
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
    const RGTexture hdr = g.create("hdr", {w, h, gpu::Format::RGBA16Float});
    RGTexture sdf_albedo, sdf_normal, sdf_depth;

    if (shadows) {
        g.add_pass("shadow", [&](PassBuilder& b) {
            b.depth(shadow_map, gpu::LoadOp::Clear, 1.0f);
            b.side_effect(); // persistent texture read through the frame data, not the graph
        }, [&](PassContext& ctx) {
            ctx.cmd.bind_pipeline(m.shadow);
            draw_meshes(ctx.cmd, false);
        });
    }
    g.add_pass("gbuffer", [&](PassBuilder& b) {
        b.color(albedo, gpu::LoadOp::Clear, {0, 0, 0, 1});
        b.color(normal, gpu::LoadOp::Clear, {0, 1, 0, 0});
        b.color(emissive, gpu::LoadOp::Clear, {0, 0, 0, 0});
        b.depth(depth, gpu::LoadOp::Clear, 1.0f);
    }, [&](PassContext& ctx) {
        ctx.cmd.bind_pipeline(m.gbuffer);
        draw_meshes(ctx.cmd, true);
    });
    if (counts_sdf > 0) {
        sdf_albedo = g.create("sdf.albedo", {w, h, gpu::Format::RGBA8Unorm});
        sdf_normal = g.create("sdf.normal", {w, h, gpu::Format::RGBA16Float});
        sdf_depth = g.create("sdf.depth", {w, h, gpu::Format::R32Float});
        g.add_pass("sdf", [&](PassBuilder& b) {
            b.read(depth);
            b.write_storage(sdf_albedo);
            b.write_storage(sdf_normal);
            b.write_storage(sdf_depth);
        }, [&](PassContext& ctx) {
            const SdfPush push{frame_address, shape_address, ctx.bindless(depth), ctx.bindless(sdf_albedo),
                               ctx.bindless(sdf_normal), ctx.bindless(sdf_depth)};
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
    g.add_pass("tonemap", [&](PassBuilder& b) {
        b.read(hdr);
        b.color(output, gpu::LoadOp::DontCare);
    }, [&](PassContext& ctx) {
        const TonemapPush push{ctx.bindless(hdr), scene.environment.exposure, is_srgb(m.target_format) ? 0u : 1u};
        ctx.cmd.bind_pipeline(m.tonemap);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.draw(3);
    });
    g.execute(cmd);

    m.stats.sdf_shapes = counts_sdf;
    m.stats.point_lights = static_cast<std::uint32_t>(m.lights.size());
    m.stats.passes = g.stats().passes_executed;
}

} // namespace astral
