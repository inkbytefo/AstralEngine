#pragma once

#include "frame/frame_slot.hpp"
#include "gpu_layouts.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"

#include <astral/core/math.hpp>
#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace astral {

struct TransparentDraw {
    MeshHandle mesh;
    std::uint32_t instance = 0;
    ShaderHandle shader;
};

struct SceneTlas {
    SceneTlas(gpu::Device& device, std::uint32_t capacity);
    ~SceneTlas();

    gpu::Device& device;
    gpu::AccelerationStructureHandle handle;
    std::uint32_t capacity;
    std::vector<gpu::TlasInstance> submitted_instances;
    bool submitted = false;
};

struct FrameInstances {
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> blended;
    std::vector<std::uint32_t> instance_shader; // per drawn opaque instance: shader index + 1 (0 = standard)
    std::vector<GpuInstance> instances;
    std::vector<TransparentDraw> transparent_draws; ///< Back to front, this frame.
    std::vector<GpuPointLight> lights;
    std::vector<GpuSdfShape> shapes;
    std::vector<GpuRectLight> rects;
    std::vector<GpuDecal> decals;
    std::vector<gpu::TlasInstance> tlas_instances;
    std::shared_ptr<SceneTlas> tlas;
    bool tlas_build_recorded = false;
};

/// Uploads the material table, partitions and sorts opaque/blended instances (+ TLAS instances when
/// `with_tlas`), and writes the GPU instance buffer into `slot`.
void prepare_instances(gpu::Device& device, const RenderScene& scene, const Vec3& camera_position,
                       const MeshStore& meshes, MaterialStore& materials, FrameSlot& slot, FrameInstances& out,
                       bool with_tlas);

/// Packs point lights, SDF shapes, decals (and rect lights when `with_rects`) into `slot`.
void upload_scene_buffers(gpu::Device& device, const RenderScene& scene, const TextureStore& textures, FrameSlot& slot,
                          FrameInstances& out, bool with_rects);

/// Grows the scene TLAS if needed and records its build into `cmd`. Returns false if TLAS creation failed.
bool build_tlas(gpu::Device& device, gpu::CommandList& cmd, FrameInstances& frame_instances);

} // namespace astral
