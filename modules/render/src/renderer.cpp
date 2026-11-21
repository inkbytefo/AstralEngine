#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <shaders/bloom_down.comp.hpp>
#include <shaders/bloom_up.comp.hpp>
#include <shaders/composite.comp.hpp>
#include <shaders/ddgi_trace.comp.hpp>
#include <shaders/ddgi_update.comp.hpp>
#include <shaders/denoise_atrous.comp.hpp>
#include <shaders/denoise_temporal.comp.hpp>
#include <shaders/gbuffer.frag.hpp>
#include <shaders/gbuffer.vert.hpp>
#include <shaders/lighting.comp.hpp>
#include <shaders/rt_lighting.comp.hpp>
#include <shaders/sdf.comp.hpp>
#include <shaders/shadow.vert.hpp>
#include <shaders/taa.comp.hpp>
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
    gpu::AccelerationStructureHandle blas; // ray-traced path only
    std::uint32_t index_count = 0;
    Vec3 bounds_min{}, bounds_max{}; // local-space AABB
};
struct GpuInstance {
    Mat4 model;
    Mat4 prev_model;
    std::uint32_t material = 0;
    std::uint32_t vertices[2] = {0, 0}; // 64-bit addresses as two words (4-byte aligned struct)
    std::uint32_t indices[2] = {0, 0};
};
static_assert(sizeof(GpuInstance) == 148);
struct GpuMaterial {
    Vec4 base_color;
    Vec4 emissive_roughness;
    Vec4 params;
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
    Mat4 prev_view_projection;
    Vec4 ddgi_origin;
    Vec4 ddgi_spacing;
    std::uint32_t ddgi_counts[4];
    std::uint32_t rt_counts[4];
    Vec4 rt_params;
    Mat4 unjittered_view_projection;
};
struct GpuPointLight {
    Vec4 position_radius;
    Vec4 color_intensity;
    Vec4 source;
};
struct GpuRectLight {
    Vec4 position_two_sided;
    Vec4 right;
    Vec4 up;
    Vec4 radiance;
};
struct GBufferIds {
    std::uint32_t albedo, normal, emissive, depth, velocity, sdf_albedo, sdf_normal, sdf_depth, sdf_velocity;
};
struct RtLightingPush {
    std::uint64_t frame, tlas, instances, materials, lights, rects, shapes;
    GBufferIds g;
    std::uint32_t irr, dist, out_diffuse, out_specular, out_geometry;
    float near_gi_radius;
};
struct DdgiTracePush {
    std::uint64_t frame, tlas, instances, materials, lights, rects, shapes, rays;
    std::uint32_t irr_prev, dist_prev;
};
struct DdgiUpdatePush {
    std::uint64_t frame, rays;
    std::uint32_t prev, out, mode;
};
struct TemporalPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t geometry, prev_geometry, diffuse, specular, prev_diffuse, prev_specular, out_diffuse, out_specular;
    float max_history;
    std::uint32_t prev_moments, out_moments;
};
struct AtrousPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t geometry, diffuse, specular, out_diffuse, out_specular;
    std::int32_t step;
    std::uint32_t moments;
};
struct CompositePush {
    std::uint64_t frame, rects;
    GBufferIds g;
    std::uint32_t diffuse, specular, out_hdr, debug_view;
};
struct TaaPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t hdr, history, out_color;
    float blend;
};
struct BloomDownPush {
    std::uint32_t src, dst;
    float texel[2];
    std::uint32_t size[2];
    std::uint32_t first;
};
struct BloomUpPush {
    std::uint32_t low, current, dst;
    float texel[2];
    std::uint32_t size[2];
    float radius;
};
static_assert(sizeof(RtLightingPush) <= gpu::max_push_constant_size);
static_assert(sizeof(DdgiTracePush) <= gpu::max_push_constant_size);
static_assert(sizeof(TemporalPush) <= gpu::max_push_constant_size);
struct GpuSdfShape {
    Vec4 position_blend;
    Vec4 rotation;
    Vec4 size;
    Vec4 albedo_roughness;
    Vec4 params;
};
struct MeshPush {
    std::uint64_t vertices, instances, frame, materials;
    std::uint32_t first_instance;
};
struct SdfPush {
    std::uint64_t frame, shapes;
    std::uint32_t mesh_depth, out_albedo, out_normal, out_depth, out_velocity;
};
struct LightingPush {
    std::uint64_t frame, lights, shapes;
    std::uint32_t albedo, normal, emissive, depth, sdf_albedo, sdf_normal, sdf_depth, out_hdr;
};
struct TonemapPush {
    std::uint32_t hdr;
    float exposure;
    std::uint32_t encode_srgb;
    std::uint32_t bloom = 0xFFFFFFFFu;
    float bloom_strength = 0.0f;
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
    GrowBuffer instances, lights, shapes, frame, rects, materials;
    std::shared_ptr<std::uint64_t> last_use = std::make_shared<std::uint64_t>(0);
};

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

std::array<float, 12> row_major_3x4(const Mat4& m) {
    return {m[0].x, m[1].x, m[2].x, m[3].x, m[0].y, m[1].y, m[2].y, m[3].y, m[0].z, m[1].z, m[2].z, m[3].z};
}

} // namespace

struct Renderer::Impl {
    gpu::Device& device;
    gpu::Format target_format;
    RenderSettings settings;
    RenderGraph graph;
    HandlePool<GpuMesh, MeshTag> meshes;
    HandlePool<MaterialDesc, MaterialTag> materials;
    MaterialHandle default_material;
    std::uint32_t material_slots = 0; ///< Highest material index + 1 (size of the GPU table).
    std::vector<GpuMaterial> material_table;
    gpu::PipelineHandle gbuffer, shadow, sdf, lighting, tonemap;
    gpu::PipelineHandle rt_lighting, ddgi_trace, ddgi_update, temporal, atrous, composite, taa, bloom_down, bloom_up;
    gpu::TextureHandle shadow_map;
    std::array<FrameSlot, ring_size> ring{};
    std::uint32_t ring_index = 0;
    RenderStats stats;
    std::vector<std::uint32_t> order;
    std::vector<GpuInstance> instances;
    std::vector<GpuPointLight> lights;
    std::vector<GpuSdfShape> shapes;
    std::vector<GpuRectLight> rects;
    std::vector<gpu::TlasInstance> tlas_instances;

    // Ray-traced path state.
    bool rt = false;
    gpu::AccelerationStructureHandle tlas;
    std::uint32_t tlas_capacity = 0;
    HistoryTextures history;
    DdgiState ddgi;
    std::uint32_t frame_index = 0;
    bool history_valid = false;
    Mat4 prev_view_projection = Mat4::identity();

    Impl(gpu::Device& d, gpu::Format f, const RenderSettings& s) : device(d), target_format(f), settings(s), graph(d) {}

    /// Sorted instance list (+ TLAS instances when `rt`) and the material table, into `slot`.
    void prepare_instances(const RenderScene& scene, FrameSlot& slot, bool with_tlas);
    bool create_rt_pipelines();
    void destroy_rt_pipelines();

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
    m.shadow_map = device.create_texture({.width = settings.shadow_map_size, .height = settings.shadow_map_size,
                                          .format = gpu::Format::D32Float,
                                          .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled,
                                          .debug_name = "astral.shadow_map"});
    m.default_material = m.materials.create(MaterialDesc{});
    m.material_slots = m.default_material.index + 1;
    m.rt = settings.ray_tracing && device.supports(gpu::Feature::RayTracing) && m.create_rt_pipelines();
}

bool Renderer::Impl::create_rt_pipelines() {
    if (rt_lighting.valid()) return true;
    auto compute = [&](std::span<const std::uint32_t> spirv, const char* name) {
        return device.create_compute_pipeline({.shader = {spirv}, .debug_name = name});
    };
    rt_lighting = compute(shaders::rt_lighting_comp, "astral.rt_lighting");
    ddgi_trace = compute(shaders::ddgi_trace_comp, "astral.ddgi_trace");
    ddgi_update = compute(shaders::ddgi_update_comp, "astral.ddgi_update");
    temporal = compute(shaders::denoise_temporal_comp, "astral.denoise_temporal");
    atrous = compute(shaders::denoise_atrous_comp, "astral.denoise_atrous");
    composite = compute(shaders::composite_comp, "astral.composite");
    taa = compute(shaders::taa_comp, "astral.taa");
    bloom_down = compute(shaders::bloom_down_comp, "astral.bloom_down");
    bloom_up = compute(shaders::bloom_up_comp, "astral.bloom_up");
    for (auto p : {rt_lighting, ddgi_trace, ddgi_update, temporal, atrous, composite, taa, bloom_down, bloom_up}) {
        if (!p.valid()) {
            destroy_rt_pipelines(); // fall back to the raster path
            return false;
        }
    }
    return true;
}

void Renderer::Impl::destroy_rt_pipelines() {
    for (auto* p : {&rt_lighting, &ddgi_trace, &ddgi_update, &temporal, &atrous, &composite, &taa, &bloom_down, &bloom_up}) {
        device.destroy(*p);
        *p = {};
    }
}

void Renderer::Impl::prepare_instances(const RenderScene& scene, FrameSlot& slot, bool with_tlas) {
    // Material table indexed by handle slot; dead slots keep the default material's values.
    material_table.assign(material_slots, {});
    const MaterialDesc* fallback = materials.get(default_material);
    for (auto& entry : material_table) {
        entry = {fallback->base_color, {fallback->emissive.x, fallback->emissive.y, fallback->emissive.z, fallback->roughness},
                 {fallback->metallic, 0, 0, 0}};
    }
    materials.for_each([&](MaterialHandle h, MaterialDesc& d) {
        material_table[h.index] = {d.base_color, {d.emissive.x, d.emissive.y, d.emissive.z, d.roughness}, {d.metallic, 0, 0, 0}};
    });
    slot.materials.write(device, material_table.data(), material_table.size() * sizeof(GpuMaterial));

    order.clear();
    for (std::uint32_t i = 0; i < scene.meshes.size(); ++i) {
        if (meshes.get(scene.meshes[i].mesh)) order.push_back(i); // destroyed meshes are skipped
    }
    std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return scene.meshes[a].mesh.index < scene.meshes[b].mesh.index;
    });
    instances.clear();
    tlas_instances.clear();
    for (std::uint32_t i : order) {
        const MeshInstance& d = scene.meshes[i];
        const GpuMesh* mesh = meshes.get(d.mesh);
        const std::uint32_t material = materials.get(d.material) ? d.material.index : default_material.index;
        const auto split = [](std::uint64_t a, std::uint32_t (&out)[2]) {
            out[0] = static_cast<std::uint32_t>(a);
            out[1] = static_cast<std::uint32_t>(a >> 32);
        };
        GpuInstance gi{d.transform, d.previous_transform.value_or(d.transform), material, {0, 0}, {0, 0}};
        split(device.buffer_address(mesh->vertices), gi.vertices);
        split(device.buffer_address(mesh->indices), gi.indices);
        const auto index = static_cast<std::uint32_t>(instances.size());
        instances.push_back(gi);
        if (with_tlas && mesh->blas.valid()) {
            tlas_instances.push_back({.blas = mesh->blas, .transform = row_major_3x4(d.transform), .custom_index = index});
        }
    }
    slot.instances.write(device, instances.data(), instances.size() * sizeof(GpuInstance));
}

Renderer::~Renderer() {
    auto& m = *impl_;
    m.meshes.for_each([&](MeshHandle, GpuMesh& mesh) {
        m.device.destroy(mesh.vertices);
        m.device.destroy(mesh.indices);
        m.device.destroy(mesh.blas);
    });
    for (auto& slot : m.ring) {
        for (GrowBuffer* b : {&slot.instances, &slot.lights, &slot.shapes, &slot.frame, &slot.rects, &slot.materials}) {
            m.device.destroy(b->buffer);
        }
    }
    for (auto p : {m.gbuffer, m.shadow, m.sdf, m.lighting, m.tonemap, m.rt_lighting, m.ddgi_trace, m.ddgi_update,
                   m.temporal, m.atrous, m.composite, m.taa, m.bloom_down, m.bloom_up}) {
        m.device.destroy(p);
    }
    m.device.destroy(m.shadow_map);
    m.device.destroy(m.tlas);
    m.history.destroy(m.device);
    m.ddgi.destroy(m.device);
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
    if (m.rt) {
        mesh.blas = m.device.create_blas({.vertices = mesh.vertices,
                                          .vertex_count = static_cast<std::uint32_t>(data.vertices.size()),
                                          .vertex_stride = sizeof(Vertex),
                                          .indices = mesh.indices,
                                          .index_count = mesh.index_count,
                                          .debug_name = "astral.mesh_blas"});
    }
    return m.meshes.create(mesh);
}

void Renderer::destroy(MeshHandle handle) {
    auto& m = *impl_;
    if (const GpuMesh* mesh = m.meshes.get(handle)) {
        m.device.destroy(mesh->vertices);
        m.device.destroy(mesh->indices);
        m.device.destroy(mesh->blas);
        m.meshes.destroy(handle);
    }
}

const RenderStats& Renderer::stats() const noexcept { return impl_->stats; }

MaterialHandle Renderer::create_material(const MaterialDesc& desc) {
    auto& m = *impl_;
    const MaterialHandle h = m.materials.create(desc);
    m.material_slots = std::max(m.material_slots, h.index + 1);
    return h;
}

bool Renderer::update_material(MaterialHandle material, const MaterialDesc& desc) {
    MaterialDesc* d = impl_->materials.get(material);
    if (!d) return false;
    *d = desc;
    return true;
}

void Renderer::destroy(MaterialHandle material) {
    if (material == impl_->default_material) return; // always valid
    impl_->materials.destroy(material);
}

const MaterialDesc* Renderer::material(MaterialHandle material) const noexcept { return impl_->materials.get(material); }

MaterialHandle Renderer::default_material() const noexcept { return impl_->default_material; }

const RenderSettings& Renderer::settings() const noexcept { return impl_->settings; }

void Renderer::set_settings(const RenderSettings& settings) {
    auto& m = *impl_;
    const RenderSettings old = m.settings;
    m.settings = settings;
    const bool want_rt = settings.ray_tracing && m.device.supports(gpu::Feature::RayTracing);
    if (want_rt != m.rt) {
        m.rt = want_rt && m.create_rt_pipelines();
        // Meshes created on the raster path have no BLAS yet.
        if (m.rt) {
            m.meshes.for_each([&](MeshHandle, GpuMesh& mesh) {
                if (mesh.blas.valid()) return;
                const auto vertex_count = static_cast<std::uint32_t>(m.device.buffer_size(mesh.vertices) / sizeof(Vertex));
                mesh.blas = m.device.create_blas({.vertices = mesh.vertices, .vertex_count = vertex_count,
                                                  .vertex_stride = sizeof(Vertex), .indices = mesh.indices,
                                                  .index_count = mesh.index_count, .debug_name = "astral.mesh_blas"});
            });
        }
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
    FrameSlot& slot = m.ring[m.ring_index];
    m.ring_index = (m.ring_index + 1) % ring_size;
    m.device.wait({*slot.last_use});
    cmd.on_submitted([use = slot.last_use](gpu::SubmitTicket t) { *use = t.value; });

    m.prepare_instances(scene, slot, false);
    m.lights.clear();
    for (const PointLight& l : scene.lights) {
        m.lights.push_back({{l.position.x, l.position.y, l.position.z, l.radius},
                            {l.color.x, l.color.y, l.color.z, l.intensity},
                            {l.source_radius, 0, 0, 0}});
    }
    m.shapes.clear();
    for (const SdfShape& s : scene.sdf) {
        m.shapes.push_back({{s.position.x, s.position.y, s.position.z, s.blend},
                            {s.rotation.x, s.rotation.y, s.rotation.z, s.rotation.w},
                            s.size,
                            {s.color.x, s.color.y, s.color.z, s.roughness},
                            {s.metallic, static_cast<float>(s.operation), static_cast<float>(s.primitive), 0}});
    }
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
                   {static_cast<float>(w), static_cast<float>(h), 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h)},
                   Mat4::identity(), {}, {}, {}, {}, {}, view_projection};
    slot.frame.write(m.device, &frame, sizeof(frame));

    const std::uint64_t instance_address = m.device.buffer_address(slot.instances.buffer);
    const std::uint64_t material_address = m.device.buffer_address(slot.materials.buffer);
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
                const MeshPush push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, material_address, start};
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
            ctx.cmd.bind_pipeline(m.shadow);
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
        ctx.cmd.bind_pipeline(m.gbuffer);
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
    FrameSlot& slot = m.ring[m.ring_index];
    m.ring_index = (m.ring_index + 1) % ring_size;
    m.device.wait({*slot.last_use});
    cmd.on_submitted([use = slot.last_use](gpu::SubmitTicket t) { *use = t.value; });

    ensure_history(w, h);
    ensure_ddgi(scene);

    // --- Scene data ------------------------------------------------------------------------
    m.prepare_instances(scene, slot, true);
    m.lights.clear();
    for (const PointLight& l : scene.lights) {
        m.lights.push_back({{l.position.x, l.position.y, l.position.z, l.radius},
                            {l.color.x, l.color.y, l.color.z, l.intensity},
                            {l.source_radius, 0, 0, 0}});
    }
    m.shapes.clear();
    for (const SdfShape& sh : scene.sdf) {
        m.shapes.push_back({{sh.position.x, sh.position.y, sh.position.z, sh.blend},
                            {sh.rotation.x, sh.rotation.y, sh.rotation.z, sh.rotation.w},
                            sh.size,
                            {sh.color.x, sh.color.y, sh.color.z, sh.roughness},
                            {sh.metallic, static_cast<float>(sh.operation), static_cast<float>(sh.primitive), 0}});
    }
    m.rects.clear();
    for (const RectLight& r : scene.rect_lights) {
        m.rects.push_back({{r.position.x, r.position.y, r.position.z, r.two_sided ? 1.0f : 0.0f},
                           {r.right.x, r.right.y, r.right.z, 0},
                           {r.up.x, r.up.y, r.up.z, 0},
                           {r.color.x * r.intensity, r.color.y * r.intensity, r.color.z * r.intensity, 0}});
    }
    slot.lights.write(m.device, m.lights.data(), m.lights.size() * sizeof(GpuPointLight));
    slot.shapes.write(m.device, m.shapes.data(), m.shapes.size() * sizeof(GpuSdfShape));
    slot.rects.write(m.device, m.rects.data(), m.rects.size() * sizeof(GpuRectLight));

    // TLAS grows in powers of two; the old one is destroyed once the GPU is done with it.
    const auto needed = static_cast<std::uint32_t>(m.tlas_instances.size());
    if (!m.tlas.valid() || needed > m.tlas_capacity) {
        m.device.destroy(m.tlas);
        m.tlas_capacity = 64;
        while (m.tlas_capacity < needed) m.tlas_capacity *= 2;
        m.tlas = m.device.create_tlas({.max_instances = m.tlas_capacity, .debug_name = "astral.scene_tlas"});
        if (!m.tlas.valid()) return;
    }
    cmd.build_tlas(m.tlas, m.tlas_instances);

    // --- Frame constants: jittered projection for TAA -------------------------------------
    const std::uint32_t jitter_index = (m.frame_index % 16) + 1;
    const float jx = (halton(jitter_index, 2) - 0.5f), jy = (halton(jitter_index, 3) - 0.5f);
    const Mat4 unjittered = camera.projection * camera.view;
    const Mat4 jittered = translate({2.0f * jx / static_cast<float>(w), 2.0f * jy / static_cast<float>(h), 0}) * unjittered;
    const Vec3 sun = normalize(scene.sun.direction);
    const auto sdf_count = static_cast<std::uint32_t>(m.shapes.size());
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
    frame.counts[0] = static_cast<std::uint32_t>(m.lights.size());
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
    frame.rt_counts[0] = static_cast<std::uint32_t>(m.rects.size());
    frame.rt_counts[1] = m.frame_index;
    frame.rt_counts[2] = m.history_valid ? 1u : 0u;
    frame.rt_counts[3] = ddgi.valid ? 1u : 0u;
    frame.rt_params = {std::max(scene.sun.angular_radius, 0.0f), jx, jy, settings.max_ray_distance};
    frame.unjittered_view_projection = unjittered;
    slot.frame.write(m.device, &frame, sizeof(frame));

    const std::uint64_t frame_address = m.device.buffer_address(slot.frame.buffer);
    const std::uint64_t instance_address = m.device.buffer_address(slot.instances.buffer);
    const std::uint64_t material_address = m.device.buffer_address(slot.materials.buffer);
    const std::uint64_t light_address = m.device.buffer_address(slot.lights.buffer);
    const std::uint64_t shape_address = m.device.buffer_address(slot.shapes.buffer);
    const std::uint64_t rect_address = m.device.buffer_address(slot.rects.buffer);
    const std::uint64_t tlas_address = m.device.acceleration_structure_address(m.tlas);
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

    auto draw_meshes = [&](gpu::CommandList& c) {
        std::uint32_t start = 0;
        while (start < m.order.size()) {
            const MeshHandle handle = scene.meshes[m.order[start]].mesh;
            std::uint32_t end = start + 1;
            while (end < m.order.size() && scene.meshes[m.order[end]].mesh == handle) ++end;
            const GpuMesh* mesh = m.meshes.get(handle);
            const MeshPush push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, material_address, start};
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
        ctx.cmd.bind_pipeline(m.gbuffer);
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
        ctx.cmd.bind_pipeline(m.ddgi_trace);
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
            ctx.cmd.bind_pipeline(m.ddgi_update);
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
        ctx.cmd.bind_pipeline(m.rt_lighting);
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
        ctx.cmd.bind_pipeline(m.temporal);
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
            ctx.cmd.bind_pipeline(m.atrous);
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
        ctx.cmd.bind_pipeline(m.composite);
        ctx.cmd.push_constants(&push, sizeof(push));
        ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    g.add_pass("taa", [&](PassBuilder& b) {
        gbuffer_reads(b);
        b.read(hdr);
        b.read(taa_prev);
        b.write_storage(taa_out);
    }, [&](PassContext& ctx) {
        const TaaPush push{frame_address, gbuffer_ids(ctx), ctx.bindless(hdr), ctx.bindless(taa_prev),
                           ctx.bindless(taa_out), std::clamp(settings.taa_blend, 0.01f, 1.0f)};
        ctx.cmd.bind_pipeline(m.taa);
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
            ctx.cmd.bind_pipeline(m.bloom_down);
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
                ctx.cmd.bind_pipeline(m.bloom_up);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((size.width + 7) / 8, (size.height + 7) / 8);
            });
            low = dst;
        }
        bloom_result = low;
    }
    g.add_pass("tonemap", [&](PassBuilder& b) {
        b.read(taa_out);
        if (bloom_result.valid()) b.read(bloom_result);
        b.color(output, gpu::LoadOp::DontCare);
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
    g.execute(cmd);

    m.prev_view_projection = unjittered;
    m.history_valid = true;
    ddgi.valid = true;
    ++m.frame_index;
    m.stats.ray_traced = true;
    m.stats.sdf_shapes = sdf_count;
    m.stats.point_lights = static_cast<std::uint32_t>(m.lights.size());
    m.stats.rect_lights = static_cast<std::uint32_t>(m.rects.size());
    m.stats.gi_probes = ddgi.nx * ddgi.ny * ddgi.nz;
    m.stats.passes = g.stats().passes_executed;
}

} // namespace astral
