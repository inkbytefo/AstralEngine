#include "studio.hpp"

namespace asset_viewer {

using namespace astral;

RenderSettings studio_settings() {
    RenderSettings settings;
    settings.gi = {.probes_x = 8,
                   .probes_y = 5,
                   .probes_z = 8,
                   .rays_per_probe = 96,
                   .volume_min = {-4.0f, 0.0f, -4.0f},
                   .volume_max = {4.0f, 3.0f, 4.0f}};
    settings.bloom_strength = 0.04f;
    return settings;
}

RenderSettings scene_settings() {
    RenderSettings settings;
    settings.bloom_strength = 0.05f;
    return settings;
}

Entity build_model_studio(Context& ctx, std::string_view model_file, bool raster, MaterialHandle& floor_material,
                          std::vector<Entity>& owned) {
    const Studio studio;
    if (!floor_material.valid()) {
        floor_material = ctx.materials().create_material({.base_color = {0.5f, 0.5f, 0.52f, 1.0f}, .roughness = 0.55f});
    }
    owned.push_back(ctx.world.create(Name{"model"}, Transform{}, ModelRef{std::string(model_file)}));
    owned.push_back(ctx.world.create(Name{"studio floor"},
                                     Transform{.position = {0.0f, -0.5f, 0.0f}, .scale = {12.0f, 1.0f, 12.0f}},
                                     MeshRenderer{ctx.meshes.plane, floor_material}));
    owned.push_back(ctx.world.create(Name{"key light"}, Transform{.position = studio.key.position},
                                     RectLight{studio.key}));
    if (raster) {
        owned.push_back(ctx.world.create(Name{"raster key fill"}, Transform{.position = studio.key.position},
                                         PointLight{.position = studio.key.position,
                                                    .color = studio.key.color,
                                                    .intensity = 16.0f,
                                                    .radius = 14.0f}));
    }
    const Quat rot = rotation_of(inverse(look_at(studio.eye, studio.target, {0.0f, 1.0f, 0.0f})));
    const Entity cam = ctx.world.create(Name{"studio camera"}, Transform{.position = studio.eye, .rotation = rot},
                                        Camera{.fov_y = radians(40.0f), .near_plane = 0.05f});
    owned.push_back(cam);
    ctx.sun = studio.sun;
    ctx.environment = studio.environment;
    return cam;
}

} // namespace asset_viewer
