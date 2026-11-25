#include "render_extract.hpp"
#include "vfx_runtime.hpp"

#include <astral/app/components.hpp>
#include <astral/app/gameplay_smoke.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>

namespace astral {

namespace {

/// Internal component: the transform a MeshRenderer entity was drawn with last frame.
struct RenderHistory {
    Mat4 previous;
};

void extract_meshes(World& world, RenderScratch& scratch) {
    scratch.draw_items.clear();
    scratch.new_histories.clear();
    world.each<const MeshRenderer, const WorldTransform>([&](Entity e, const MeshRenderer& r, const WorldTransform& wt) {
        MeshInstance inst{.mesh = r.mesh, .material = r.material, .transform = wt.matrix};
        if (RenderHistory* h = world.get<RenderHistory>(e)) {
            inst.previous_transform = h->previous;
            h->previous = wt.matrix;
        } else {
            scratch.new_histories.emplace_back(e, wt.matrix);
        }
        scratch.draw_items.push_back(inst);
    });
    for (const auto& [e, matrix] : scratch.new_histories) world.add(e, RenderHistory{matrix});
}

void extract_sdfs(World& world, RenderScratch& scratch) {
    scratch.sdf_sorted.clear();
    world.each<const Sdf>([&](Entity e, const Sdf& s) {
        SdfShape shape = s.shape;
        if (const WorldTransform* wt = world.get<const WorldTransform>(e)) {
            shape.position = {wt->matrix[3].x, wt->matrix[3].y, wt->matrix[3].z};
            shape.rotation = rotation_of(wt->matrix);
        }
        scratch.sdf_sorted.emplace_back(s.order, shape);
    });
    std::stable_sort(scratch.sdf_sorted.begin(), scratch.sdf_sorted.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    scratch.sdf_shapes.clear();
    scratch.sdf_shapes.reserve(scratch.sdf_sorted.size());
    for (const auto& [order, shape] : scratch.sdf_sorted) scratch.sdf_shapes.push_back(shape);
}

void extract_lights_and_decals(World& world, RenderScratch& scratch) {
    scratch.point_lights.clear();
    world.each<const PointLight>([&](Entity e, const PointLight& l) {
        PointLight light = l;
        if (const WorldTransform* wt = world.get<const WorldTransform>(e)) {
            light.position = {wt->matrix[3].x, wt->matrix[3].y, wt->matrix[3].z};
        }
        scratch.point_lights.push_back(light);
    });

    scratch.rect_lights.clear();
    world.each<const RectLight>([&](Entity e, const RectLight& l) {
        RectLight light = l;
        if (const WorldTransform* wt = world.get<const WorldTransform>(e)) {
            light.position = {wt->matrix[3].x, wt->matrix[3].y, wt->matrix[3].z};
            const Vec4 r = wt->matrix * Vec4{l.right.x, l.right.y, l.right.z, 0};
            const Vec4 u = wt->matrix * Vec4{l.up.x, l.up.y, l.up.z, 0};
            light.right = {r.x, r.y, r.z};
            light.up = {u.x, u.y, u.z};
        }
        scratch.rect_lights.push_back(light);
    });

    scratch.decals.clear();
    world.each<const Decal>([&](Entity e, const Decal& d) {
        Decal decal = d;
        if (const WorldTransform* wt = world.get<const WorldTransform>(e)) decal.transform = wt->matrix;
        scratch.decals.push_back(decal);
    });
}

void extract_vfx(World& world, RenderScratch& scratch) {
    scratch.smoke_volumes.clear();
    world.each<const GameplaySmoke, const Transform>([&](Entity e, const GameplaySmoke& cloud, const Transform& t) {
        if (!world.has<Parent>(e) && cloud.extinction_per_m() > 0) {
            const auto* appearance = world.get<const SmokeAppearance>(e);
            scratch.smoke_volumes.push_back({t.position, cloud.cell_size_m(), cloud.extinction_per_m(), cloud.side(),
                                             cloud.occupancy(), appearance ? appearance->material : MaterialHandle{}});
        }
    });
    world.each<const VfxVolume, const Transform>([&](const VfxVolume& volume, const Transform& t) {
        if (volume.material.valid()) {
            scratch.smoke_volumes.push_back({t.position, volume.cell_size_m, volume.strength, VfxVolume::side, {},
                                             volume.material, volume.age});
        }
    });

    scratch.particle_bursts.clear();
    world.each<const VfxEmitter, const Transform>(
        [&](Entity entity, const VfxEmitter& emitter, const Transform& transform) {
            ParticleBurst burst = emitter.burst;
            burst.position = transform.position;
            burst.simulation_id = (std::uint64_t{entity.generation} << 32) | entity.index;
            scratch.particle_bursts.push_back(burst);
        });
}

} // namespace

CameraData find_active_camera(World& world, float aspect) {
    CameraData data;
    data.position = {0, 2, 6};
    data.view = look_at(data.position, {0, 0, 0}, {0, 1, 0});
    data.projection = perspective(radians(70.0f), aspect, 0.1f, 1000.0f);
    bool found = false;
    world.each<const Camera, const WorldTransform>([&](const Camera& cam, const WorldTransform& w) {
        if (found || !cam.active) return;
        found = true;
        data.position = {w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
        data.view = inverse(w.matrix);
        data.projection = perspective(cam.fov_y, aspect, cam.near_plane, cam.far_plane);
    });
    return data;
}

void extract_render_scene(World& world, RenderScratch& scratch) {
    extract_meshes(world, scratch);
    extract_sdfs(world, scratch);
    extract_lights_and_decals(world, scratch);
    extract_vfx(world, scratch);
}

void shift_render_history(World& world, Vec3 offset) {
    world.each<RenderHistory>([&](RenderHistory& h) {
        h.previous[3].x -= offset.x;
        h.previous[3].y -= offset.y;
        h.previous[3].z -= offset.z;
    });
}

} // namespace astral
