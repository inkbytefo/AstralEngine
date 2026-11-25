#pragma once
// Internal to Astral::App: reusable frame extraction scratch buffers and ECS -> RenderScene extraction.

#include <astral/render/renderer.hpp>
#include <astral/world/world.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace astral {

struct RenderScratch {
    std::vector<MeshInstance> draw_items;
    std::vector<std::pair<Entity, Mat4>> new_histories;
    std::vector<std::pair<std::int32_t, SdfShape>> sdf_sorted;
    std::vector<SdfShape> sdf_shapes;
    std::vector<PointLight> point_lights;
    std::vector<RectLight> rect_lights;
    std::vector<Decal> decals;
    std::vector<SmokeVolumeView> smoke_volumes;
    std::vector<ParticleBurst> particle_bursts;
};

[[nodiscard]] CameraData find_active_camera(World& world, float aspect);
void extract_render_scene(World& world, RenderScratch& scratch);
void shift_render_history(World& world, Vec3 offset);

} // namespace astral
