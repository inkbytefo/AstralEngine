#include "frame/instances.hpp"

#include <algorithm>
#include <array>

namespace astral {

namespace {

std::array<float, 12> row_major_3x4(const Mat4& m) {
    return {m[0].x, m[1].x, m[2].x, m[3].x, m[0].y, m[1].y, m[2].y, m[3].y, m[0].z, m[1].z, m[2].z, m[3].z};
}

void split_address(std::uint64_t address, std::uint32_t (&out)[2]) {
    out[0] = static_cast<std::uint32_t>(address);
    out[1] = static_cast<std::uint32_t>(address >> 32);
}

GpuInstance pack_instance(gpu::Device& device, const MeshInstance& d, const GpuMesh& mesh, std::uint32_t material) {
    GpuInstance gi{d.transform, d.previous_transform.value_or(d.transform), material, {0, 0}, {0, 0}};
    split_address(device.buffer_address(mesh.vertices), gi.vertices);
    split_address(device.buffer_address(mesh.indices), gi.indices);
    return gi;
}

} // namespace

void prepare_instances(gpu::Device& device, const RenderScene& scene, const Vec3& camera_position,
                       const MeshStore& meshes, MaterialStore& materials, FrameSlot& slot, FrameInstances& out,
                       bool with_tlas) {
    materials.upload(slot.materials, slot.params);

    out.order.clear();
    out.blended.clear();
    for (std::uint32_t i = 0; i < scene.meshes.size(); ++i) {
        if (!meshes.get(scene.meshes[i].mesh)) continue; // destroyed meshes are skipped
        const GpuShader* gs = materials.shader_of(scene.meshes[i].material);
        if (gs && gs->kind == GpuShader::Kind::Surface && gs->blend) out.blended.push_back(i);
        else out.order.push_back(i);
    }
    // Grouped by shader (pipeline changes), then by mesh (one instanced draw per run).
    std::sort(out.order.begin(), out.order.end(), [&](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t ka = materials.surface_shader_key(scene.meshes[a].material);
        const std::uint32_t kb = materials.surface_shader_key(scene.meshes[b].material);
        if (ka != kb) return ka < kb;
        return scene.meshes[a].mesh.index < scene.meshes[b].mesh.index;
    });
    out.instance_shader.clear();
    for (std::uint32_t i : out.order) out.instance_shader.push_back(materials.surface_shader_key(scene.meshes[i].material));
    out.instances.clear();
    out.tlas_instances.clear();
    for (std::uint32_t i : out.order) {
        const MeshInstance& d = scene.meshes[i];
        const GpuMesh* mesh = meshes.get(d.mesh);
        const std::uint32_t material = materials.resolve_index(d.material);
        const auto index = static_cast<std::uint32_t>(out.instances.size());
        out.instances.push_back(pack_instance(device, d, *mesh, material));
        if (with_tlas && mesh->blas.valid()) {
            const GpuShader* gs = materials.shader_of(d.material);
            const bool is_opaque = !(gs && gs->alpha_test);
            out.tlas_instances.push_back(
                {.blas = mesh->blas, .transform = row_major_3x4(d.transform), .custom_index = index, .opaque = is_opaque});
        }
    }
    // Blended surfaces: after the opaque instances, farthest first (their own draws, one instance each).
    out.transparent_draws.clear();
    std::sort(out.blended.begin(), out.blended.end(), [&](std::uint32_t a, std::uint32_t b) {
        auto distance2 = [&](std::uint32_t i) {
            const Mat4& t = scene.meshes[i].transform;
            const Vec3 d{t[3].x - camera_position.x, t[3].y - camera_position.y, t[3].z - camera_position.z};
            return d.x * d.x + d.y * d.y + d.z * d.z;
        };
        return distance2(a) > distance2(b);
    });
    for (std::uint32_t i : out.blended) {
        const MeshInstance& d = scene.meshes[i];
        const GpuMesh* mesh = meshes.get(d.mesh);
        out.transparent_draws.push_back({d.mesh, static_cast<std::uint32_t>(out.instances.size()), materials.get(d.material)->shader});
        out.instances.push_back(pack_instance(device, d, *mesh, d.material.index));
    }
    slot.instances.write(device, out.instances.data(), out.instances.size() * sizeof(GpuInstance));
}

void upload_scene_buffers(gpu::Device& device, const RenderScene& scene, const TextureStore& textures, FrameSlot& slot,
                          FrameInstances& out, bool with_rects) {
    out.lights.clear();
    for (const PointLight& l : scene.lights) {
        out.lights.push_back({{l.position.x, l.position.y, l.position.z, l.radius},
                              {l.color.x, l.color.y, l.color.z, l.intensity},
                              {l.source_radius, 0, 0, 0}});
    }
    out.shapes.clear();
    for (const SdfShape& s : scene.sdf) {
        out.shapes.push_back({{s.position.x, s.position.y, s.position.z, s.blend},
                              {s.rotation.x, s.rotation.y, s.rotation.z, s.rotation.w},
                              s.size,
                              {s.color.x, s.color.y, s.color.z, s.roughness},
                              {s.metallic, static_cast<float>(s.operation), static_cast<float>(s.primitive), 0}});
    }
    out.decals.clear();
    for (const Decal& d : scene.decals) {
        const Vec3 up = normalize(Vec3{d.transform[1].x, d.transform[1].y, d.transform[1].z});
        out.decals.push_back({inverse(d.transform),
                              d.color,
                              {d.roughness, d.metallic, d.wetness, d.normal_strength},
                              {up.x, up.y, up.z, d.affect_roughness ? 1.0f : 0.0f},
                              {textures.bindless_or(d.albedo_texture, no_texture),
                               textures.bindless_or(d.normal_texture, no_texture), no_texture, no_texture}});
    }
    slot.lights.write(device, out.lights.data(), out.lights.size() * sizeof(GpuPointLight));
    slot.shapes.write(device, out.shapes.data(), out.shapes.size() * sizeof(GpuSdfShape));
    slot.decals.write(device, out.decals.data(), out.decals.size() * sizeof(GpuDecal));
    if (with_rects) {
        out.rects.clear();
        for (const RectLight& r : scene.rect_lights) {
            out.rects.push_back({{r.position.x, r.position.y, r.position.z, r.two_sided ? 1.0f : 0.0f},
                                 {r.right.x, r.right.y, r.right.z, 0},
                                 {r.up.x, r.up.y, r.up.z, 0},
                                 {r.color.x * r.intensity, r.color.y * r.intensity, r.color.z * r.intensity, 0}});
        }
        slot.rects.write(device, out.rects.data(), out.rects.size() * sizeof(GpuRectLight));
    }
}

SceneTlas::SceneTlas(gpu::Device& gpu_device, std::uint32_t max_instances)
    : device(gpu_device),
      handle(device.create_tlas({.max_instances = max_instances, .debug_name = "astral.scene_tlas"})),
      capacity(max_instances) {}

SceneTlas::~SceneTlas() { device.destroy(handle); }

bool build_tlas(gpu::Device& device, gpu::CommandList& cmd, FrameInstances& frame_instances) {
    auto& tlas = frame_instances.tlas;
    const auto& instances = frame_instances.tlas_instances;
    frame_instances.tlas_build_recorded = false;
    if (tlas && tlas->submitted && instances == tlas->submitted_instances) {
        // Pin the AS until this command list has been submitted or discarded.
        cmd.on_submitted([keep_alive = tlas](gpu::SubmitTicket) {});
        return true;
    }

    // A list recorded but not yet submitted may still read or build the current AS.
    if (!tlas || tlas.use_count() > 1 || instances.size() > tlas->capacity) {
        std::uint32_t capacity = 64;
        while (capacity < instances.size()) capacity *= 2;
        auto replacement = std::make_shared<SceneTlas>(device, capacity);
        if (!replacement->handle.valid()) return false;
        tlas = std::move(replacement);
    }
    if (!cmd.build_tlas(tlas->handle, instances)) return false;

    tlas->submitted = false;
    frame_instances.tlas_build_recorded = true;
    // Do not cache a build from a dropped or failed submission.
    cmd.on_submitted([target = tlas, snapshot = instances](gpu::SubmitTicket) mutable {
        target->submitted_instances = std::move(snapshot);
        target->submitted = true;
    });
    return true;
}

} // namespace astral
