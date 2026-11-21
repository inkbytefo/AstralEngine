#include <astral/render/renderer.hpp>

#include <shaders/mesh.frag.hpp>
#include <shaders/mesh.vert.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace astral {

namespace {

struct GpuMesh {
    gpu::BufferHandle vertices;
    gpu::BufferHandle indices;
    std::uint32_t index_count = 0;
};

struct GpuInstance {
    Mat4 model;
    Vec4 color;
};

struct GpuFrame {
    Mat4 view_projection;
    Vec4 camera_position;
    Vec4 light_direction;
    Vec4 light_color;
};

struct Push {
    std::uint64_t vertices;
    std::uint64_t instances;
    std::uint64_t frame;
    std::uint32_t first_instance;
};

constexpr std::uint32_t ring_size = 3; // frames in flight (2) + the one being recorded

} // namespace

struct Renderer::Impl {
    gpu::Device& device;
    gpu::Format color_format;
    gpu::PipelineHandle pipeline;
    HandlePool<GpuMesh, MeshTag> meshes;

    struct FrameBuffers {
        gpu::BufferHandle instances;
        std::uint64_t instance_capacity = 0;
        gpu::BufferHandle frame;
        /// Submission that last read this slot (0 = none or not submitted yet).
        std::shared_ptr<std::uint64_t> last_use = std::make_shared<std::uint64_t>(0);
    };
    std::array<FrameBuffers, ring_size> ring{};
    std::uint32_t ring_index = 0;

    gpu::TextureHandle depth;
    gpu::Extent2D depth_extent{};
    RenderStats stats;
    std::vector<std::uint32_t> order;     // scratch: draw items sorted by mesh
    std::vector<GpuInstance> instances;   // scratch, reused every frame

    Impl(gpu::Device& d, gpu::Format format) : device(d), color_format(format) {}
};

Renderer::Renderer(gpu::Device& device, gpu::Format color_format)
    : impl_(std::make_unique<Impl>(device, color_format)) {
    auto& m = *impl_;
    m.pipeline = device.create_graphics_pipeline({
        .vertex = {shaders::mesh_vert},
        .fragment = {shaders::mesh_frag},
        .color_formats = {color_format},
        .depth = {.format = gpu::Format::D32Float, .test = true, .write = true, .compare = gpu::CompareOp::Less},
        .cull = gpu::CullMode::Back,
        .debug_name = "astral.mesh",
    });
    for (auto& f : m.ring) {
        f.frame = device.create_buffer({.size = sizeof(GpuFrame), .usage = gpu::BufferUsage::Storage,
                                        .memory = gpu::MemoryUsage::CpuToGpu});
    }
}

Renderer::~Renderer() {
    auto& m = *impl_;
    m.meshes.for_each([&](MeshHandle, GpuMesh& mesh) {
        m.device.destroy(mesh.vertices);
        m.device.destroy(mesh.indices);
    });
    for (auto& f : m.ring) {
        m.device.destroy(f.instances);
        m.device.destroy(f.frame);
    }
    m.device.destroy(m.depth);
    m.device.destroy(m.pipeline);
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
        return {}; // e.g. called while a CommandList is recording
    }
    mesh.index_count = static_cast<std::uint32_t>(data.indices.size());
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
                      std::span<const DrawItem> items, const DirectionalLight& light, Vec4 clear) {
    auto& m = *impl_;
    m.stats = {};
    const gpu::TextureDesc target_desc = m.device.texture_desc(target);
    if (target_desc.width == 0) return;

    // Depth buffer follows the target size.
    if (m.depth_extent.width != target_desc.width || m.depth_extent.height != target_desc.height) {
        m.device.destroy(m.depth);
        m.depth = m.device.create_texture({.width = target_desc.width, .height = target_desc.height,
                                           .format = gpu::Format::D32Float,
                                           .usage = gpu::TextureUsage::DepthTarget,
                                           .debug_name = "astral.depth"});
        m.depth_extent = {target_desc.width, target_desc.height};
    }

    // Per-frame data goes into this frame's slot of the ring.
    auto& frame = m.ring[m.ring_index];
    m.ring_index = (m.ring_index + 1) % ring_size;
    // The GPU may still read this slot from an earlier frame: wait for exactly that submission
    // (normally long finished). Then mark it as used by whatever submits `cmd`.
    m.device.wait({*frame.last_use});
    cmd.on_submitted([use = frame.last_use](gpu::SubmitTicket t) { *use = t.value; });

    // Sort by mesh so each mesh is one instanced draw.
    m.order.resize(items.size());
    for (std::uint32_t i = 0; i < items.size(); ++i) m.order[i] = i;
    std::sort(m.order.begin(), m.order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return items[a].mesh.index < items[b].mesh.index;
    });
    auto& instances = m.instances;
    instances.clear();
    for (std::uint32_t i : m.order) instances.push_back({items[i].transform, items[i].color});

    const std::uint64_t needed = std::max<std::uint64_t>(instances.size(), 1) * sizeof(GpuInstance);
    if (frame.instance_capacity < needed) {
        m.device.destroy(frame.instances); // deferred: an older frame may still read it
        frame.instance_capacity = std::max(needed, frame.instance_capacity * 2);
        frame.instances = m.device.create_buffer({.size = frame.instance_capacity,
                                                  .usage = gpu::BufferUsage::Storage,
                                                  .memory = gpu::MemoryUsage::CpuToGpu});
    }
    if (!instances.empty()) {
        m.device.write_buffer(frame.instances, instances.data(), instances.size() * sizeof(GpuInstance));
    }
    const GpuFrame frame_data{camera.projection * camera.view,
                              {camera.position.x, camera.position.y, camera.position.z, 1},
                              {light.direction.x, light.direction.y, light.direction.z, 0},
                              {light.color.x * light.intensity, light.color.y * light.intensity,
                               light.color.z * light.intensity, light.ambient}};
    m.device.write_buffer(frame.frame, &frame_data, sizeof(frame_data));

    cmd.transition(target, gpu::ResourceState::ColorTarget);
    cmd.transition(m.depth, gpu::ResourceState::DepthTarget);
    cmd.begin_rendering({.color = {{.texture = target, .load = gpu::LoadOp::Clear,
                                    .clear = {clear.x, clear.y, clear.z, clear.w}}},
                         .depth = {.texture = m.depth, .load = gpu::LoadOp::Clear,
                                   .store = gpu::StoreOp::DontCare, .clear = 1.0f}});
    cmd.bind_pipeline(m.pipeline);

    const std::uint64_t instance_address = m.device.buffer_address(frame.instances);
    const std::uint64_t frame_address = m.device.buffer_address(frame.frame);
    std::uint32_t run_start = 0;
    while (run_start < m.order.size()) {
        const MeshHandle mesh_handle = items[m.order[run_start]].mesh;
        std::uint32_t run_end = run_start + 1;
        while (run_end < m.order.size() && items[m.order[run_end]].mesh == mesh_handle) ++run_end;
        if (const GpuMesh* mesh = m.meshes.get(mesh_handle)) {
            const Push push{m.device.buffer_address(mesh->vertices), instance_address, frame_address, run_start};
            cmd.push_constants(&push, sizeof(push));
            cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
            cmd.draw_indexed(mesh->index_count, run_end - run_start);
            ++m.stats.draw_calls;
            m.stats.instances += run_end - run_start;
        }
        run_start = run_end;
    }
    cmd.end_rendering();
}

} // namespace astral
