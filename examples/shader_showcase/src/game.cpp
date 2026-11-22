#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <cmath>

using namespace astral;

namespace shader_showcase {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    return game.install(ctx);
}

void ShowcaseGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    ctx.sun.direction = normalize(Vec3{-0.45f, -0.28f, -0.75f});
    ctx.sun.color = {1.0f, 0.70f, 0.45f};
    ctx.sun.intensity = 2.0f;
    ctx.environment.sky = {0.16f, 0.18f, 0.40f};
    ctx.environment.ground = {0.05f, 0.04f, 0.05f};
    ctx.environment.ambient = 0.30f;
    ctx.environment.exposure = 0.75f;
    ctx.sky = ctx.load_material("materials/dusk_sky.material");
    ctx.post = {ctx.load_material("materials/vignette.material")};

    // Ground and a backdrop wall (plain PBR: the standard shader is the default).
    ctx.world.create(Transform{.position = {0, 0, 0}, .scale = {40, 1, 40}},
                     PrimitiveMesh{.shape = Primitive::Plane, .material = {.base_color = {0.10f, 0.09f, 0.09f, 1}, .roughness = 0.85f}});
    ctx.world.create(Transform{.position = {0, 2.5f, -3.2f}, .scale = {16, 5, 0.3f}},
                     PrimitiveMesh{.shape = Primitive::Cube, .material = {.base_color = {0.11f, 0.12f, 0.15f, 1}, .roughness = 0.9f}});
    // Stripes behind the glass so the transparency reads.
    for (int i = 0; i < 4; ++i) {
        ctx.world.create(Transform{.position = {-2.8f + static_cast<float>(i) * 1.1f, 1.4f, -2.9f}, .scale = {0.35f, 2.8f, 0.2f}},
                         PrimitiveMesh{.shape = Primitive::Cube,
                                       .material = {.base_color = i % 2 ? Vec4{0.9f, 0.25f, 0.2f, 1} : Vec4{0.95f, 0.85f, 0.3f, 1}, .roughness = 0.7f}});
    }

    // Leaves: upright planes leaning out of one stem, each turned a little around it.
    for (int i = 0; i < 5; ++i) {
        const float k = static_cast<float>(i) - 2.0f;
        const float lean = k * 0.30f, yaw = k * 0.42f;
        const Quat turn = Quat::from_axis_angle({0, 1, 0}, yaw);
        const Quat rotation = turn * Quat::from_axis_angle({0, 0, 1}, lean) * Quat::from_axis_angle({1, 0, 0}, radians(90.0f));
        const Vec3 up = turn * Vec3{-std::sin(lean), std::cos(lean), 0.0f};
        ctx.world.create(Transform{.position = {-5.2f + up.x * 1.05f, 0.25f + up.y * 1.05f, up.z * 1.05f}, .rotation = rotation,
                                   .scale = {0.85f, 1.0f, 2.1f}},
                         PrimitiveMesh{.shape = Primitive::Plane, .material_file = "materials/leaf.material"});
    }
    ctx.world.create(Transform{.position = {-5.2f, 0.2f, 0.0f}, .scale = {0.12f, 0.4f, 0.12f}},
                     PrimitiveMesh{.shape = Primitive::Cube, .material = {.base_color = {0.18f, 0.25f, 0.08f, 1}}});

    ctx.world.create(Transform{.position = {-2.0f, 1.3f, 0.0f}, .scale = {2.2f, 2.2f, 2.2f}},
                     PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/glass.material"});
    ctx.world.create(Transform{.position = {0.45f, 1.0f, 0.6f}, .scale = {1.7f, 1.7f, 1.7f}},
                     PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/glass_blue.material"});

    ctx.world.create(Transform{.position = {2.9f, 1.0f, 0.2f}, .scale = {2.0f, 2.0f, 2.0f}},
                     PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/lava.material"});
    ctx.world.create(Transform{.position = {2.9f, 2.0f, 1.2f}}, PointLight{.color = {1.0f, 0.5f, 0.15f}, .intensity = 40.0f, .radius = 9.0f});

    ctx.world.create(Transform{.position = {5.4f, 1.5f, 0.0f}, .scale = {1.0f, 3.0f, 1.0f}},
                     PrimitiveMesh{.shape = Primitive::Cube, .material_file = "materials/hologram.material"});
    ctx.world.create(Transform{.position = {0.0f, 2.0f, 7.6f}, .rotation = Quat::from_axis_angle({1, 0, 0}, radians(-7.0f))},
                     Camera{.far_plane = 200.0f},
                     fly::FlyCamera{.speed = 4.0f, .fast_speed = 12.0f, .pitch = radians(-7.0f)});
}

} // namespace shader_showcase
