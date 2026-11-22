#include "game.hpp"

#include "studio.hpp"

using namespace astral;

namespace model_viewer {

RenderSettings studio_settings() {
    RenderSettings settings;
    settings.gi = {.probes_x = 8, .probes_y = 5, .probes_z = 8, .rays_per_probe = 96, .volume_min = {-4, 0, -4}, .volume_max = {4, 3, 4}};
    return settings;
}

void ViewerGame::on_start(Context& ctx) {
    const Studio studio;
    ctx.world.create(Transform{}, ModelRef{file_});
    ctx.world.create(Transform{.position = {0, -0.5f, 0}, .scale = {12, 1, 12}},
                     MeshRenderer{ctx.meshes.plane, ctx.create_material({.base_color = {0.5f, 0.5f, 0.52f, 1}, .roughness = 0.55f})});
    ctx.world.create(Transform{.position = studio.key.position}, RectLight{studio.key});
    ctx.world.create(Transform{.position = studio.eye, .rotation = rotation_of(inverse(look_at(studio.eye, studio.target, {0, 1, 0})))},
                     Camera{.fov_y = radians(40.0f), .near_plane = 0.05f});
    ctx.sun = studio.sun;
    ctx.environment = studio.environment;
    ctx.log.info("viewing {} — edit and re-export it: it reloads", file_);
}

} // namespace model_viewer
