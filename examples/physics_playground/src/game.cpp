#include "game.hpp"

#include "features/fly_camera/fly_camera.hpp"
#include "features/playground/playground.hpp"

#include <algorithm>
#include <cmath>

using namespace astral;

namespace physics_playground {

namespace {

constexpr std::uint32_t grid = 64;     // height samples per side
constexpr float cell = 1.5f;           // metres between samples: the terrain is 94.5 m wide
constexpr float half_size = 0.5f * static_cast<float>(grid - 1) * cell;

float smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// The terrain as a render mesh (the physics height field is made from the same heights).
MeshData terrain_mesh(const std::vector<float>& heights) {
    MeshData mesh;
    for (std::uint32_t z = 0; z < grid; ++z) {
        for (std::uint32_t x = 0; x < grid; ++x) {
            const float px = static_cast<float>(x) * cell - half_size, pz = static_cast<float>(z) * cell - half_size;
            const auto h = [&](std::uint32_t xi, std::uint32_t zi) { return heights[std::min(zi, grid - 1) * grid + std::min(xi, grid - 1)]; };
            const Vec3 normal = normalize(Vec3{h(x == 0 ? 0 : x - 1, z) - h(x + 1, z), 2.0f * cell, h(x, z == 0 ? 0 : z - 1) - h(x, z + 1)});
            mesh.vertices.push_back({{px, heights[z * grid + x], pz}, normal, {static_cast<float>(x) * 0.25f, static_cast<float>(z) * 0.25f}});
        }
    }
    for (std::uint32_t z = 0; z + 1 < grid; ++z) {
        for (std::uint32_t x = 0; x + 1 < grid; ++x) {
            const std::uint32_t i = z * grid + x;
            mesh.indices.insert(mesh.indices.end(), {i, i + grid, i + 1, i + 1, i + grid, i + grid + 1}); // counter-clockwise seen from above
        }
    }
    return mesh;
}

} // namespace

float terrain_height(float x, float z) {
    const float r = std::sqrt(x * x + z * z);
    const float ripple = 0.4f * std::sin(x * 0.35f) * std::cos(z * 0.3f);
    return -2.5f + 5.5f * smooth(10.0f, 28.0f, r) + ripple * smooth(8.0f, 20.0f, r);
}

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("playground", playground::install_playground);
    return game.install(ctx);
}

void PlaygroundGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    World& w = ctx.world;

    // Terrain: one height field for physics, the same heights as a mesh.
    std::vector<float> heights(std::size_t{grid} * grid);
    for (std::uint32_t z = 0; z < grid; ++z) {
        for (std::uint32_t x = 0; x < grid; ++x) {
            heights[z * grid + x] = terrain_height(static_cast<float>(x) * cell - half_size, static_cast<float>(z) * cell - half_size);
        }
    }
    const MeshHandle terrain = ctx.create_mesh(terrain_mesh(heights));
    w.create(Name{"terrain"}, Transform{},
             MeshRenderer{terrain, ctx.create_material({.base_color = {0.32f, 0.42f, 0.22f, 1}, .roughness = 0.9f})},
             RigidBody{.desc = {.shape = ShapeDesc::height_field(heights, grid, cell), .motion = MotionType::Static, .friction = 0.8f, .surface = 1}});

    // The lake: a water volume over the basin (its top is y = 0) and a see-through surface.
    w.create(Name{"lake"}, Transform{.position = {0, -3, 0}}, WaterBody{.half_extents = {26, 3, 26}, .density = 1000.0f, .flow = {0.25f, 0, 0.1f}, .wave_height = 0.12f, .wave_length = 7.0f, .wave_speed = 1.2f});
    w.create(Transform{.position = {0, 0, 0}, .scale = {52, 1, 52}}, PrimitiveMesh{.shape = Primitive::Plane, .material_file = "materials/water.material"});

    // Wind: gusty and turning (the rule is in playground.luau).
    w.create(Name{"wind"}, Transform{}, WindZone{.velocity = {6, 0, 0}, .gust_strength = 3.0f, .gust_frequency = 0.3f, .turbulence = 1.0f, .global = true});

    // Floaters on the lake: light crates float, stones sink, beach balls drift in the wind.
    const BodyDesc crate{.shape = ShapeDesc::box({0.3f, 0.3f, 0.3f}), .friction = 0.7f, .surface = 2};
    for (int i = 0; i < 6; ++i) {
        BodyDesc body = crate;
        body.mass = 90.0f;
        w.create(Transform{.position = {-6.0f + static_cast<float>(i) * 1.5f, 3.0f + static_cast<float>(i % 3), -4.0f}, .scale = {0.6f, 0.6f, 0.6f}},
                 MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.85f, 0.6f, 0.3f, 1}, .roughness = 0.6f})}, RigidBody{.desc = body},
                 Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f}, AirDrag{.area = 0.36f, .drag = 1.0f});
    }
    for (int i = 0; i < 3; ++i) {
        BodyDesc body = crate;
        body.mass = 420.0f; // denser than water: sinks
        w.create(Transform{.position = {4.0f + static_cast<float>(i), 4.0f, 3.0f}, .scale = {0.6f, 0.6f, 0.6f}},
                 MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.45f, 0.45f, 0.5f, 1}, .roughness = 0.5f, .metallic = 0.6f})}, RigidBody{.desc = body},
                 Buoyant{.volume = 0.216f, .height = 0.6f, .area = 0.36f});
    }
    for (int i = 0; i < 5; ++i) {
        w.create(Transform{.position = {-3.0f + static_cast<float>(i) * 1.8f, 2.0f, 6.0f}, .scale = {0.8f, 0.8f, 0.8f}},
                 MeshRenderer{ctx.meshes.sphere, ctx.create_material({.base_color = {0.95f, 0.3f + 0.12f * static_cast<float>(i), 0.25f, 1}, .roughness = 0.4f})},
                 RigidBody{.desc = {.shape = ShapeDesc::sphere(0.4f), .mass = 1.2f, .restitution = 0.6f, .linear_damping = 0.02f, .surface = 2}},
                 Buoyant{.volume = 0.0f, .height = 0.8f, .area = 0.5f, .drag = 0.8f}, AirDrag{.area = 0.5f, .drag = 0.47f});
    }

    // A plank bridge across the lake, each plank hinged to the next, both ends to posts on the shore.
    const float bridge_z = 17.0f, bridge_y = terrain_height(-14.0f, bridge_z) + 0.4f;
    constexpr int planks = 14;
    const float plank = 2.0f;
    const float start_x = -0.5f * plank * planks;
    std::vector<Entity> chain;
    for (int i = 0; i < planks; ++i) {
        chain.push_back(w.create(Transform{.position = {start_x + (static_cast<float>(i) + 0.5f) * plank, bridge_y, bridge_z}, .scale = {plank, 0.15f, 1.8f}},
                                 MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.5f, 0.35f, 0.2f, 1}, .roughness = 0.8f})},
                                 RigidBody{.desc = {.shape = ShapeDesc::box({plank * 0.5f, 0.075f, 0.9f}), .mass = 20.0f, .friction = 1.0f, .surface = 2}}));
    }
    w.create(Joint{.type = JointType::Hinge, .b = chain.front(), .anchor = {start_x, bridge_y, bridge_z}, .axis = {0, 0, 1}});
    for (int i = 0; i + 1 < planks; ++i) {
        w.create(Joint{.type = JointType::Hinge, .a = chain[static_cast<std::size_t>(i)], .b = chain[static_cast<std::size_t>(i + 1)],
                       .anchor = {start_x + static_cast<float>(i + 1) * plank, bridge_y, bridge_z}, .axis = {0, 0, 1}});
    }
    w.create(Joint{.type = JointType::Hinge, .b = chain.back(), .anchor = {start_x + plank * planks, bridge_y, bridge_z}, .axis = {0, 0, 1}});

    // A jump pad on the shore and a dropper above it; a glider in the wind.
    const float pad_x = 24.0f, pad_y = terrain_height(pad_x, 0.0f);
    w.create(Name{"pad"}, Transform{.position = {pad_x, pad_y + 0.2f, 0}, .scale = {2.4f, 0.3f, 2.4f}},
             MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.1f, 0.9f, 0.5f, 1}, .roughness = 0.3f, .emissive = {0.1f, 1.2f, 0.5f}})},
             RigidBody{.desc = {.shape = ShapeDesc::box({1.2f, 0.6f, 1.2f}), .motion = MotionType::Static, .sensor = true}}, playground::JumpPad{.speed = 14.0f});
    w.create(Name{"dropper"}, Transform{.position = {pad_x, pad_y + 7.0f, 0}}, playground::Dropper{.interval = 3.0f, .timer = 2.0f});
    const Quat pitch = Quat::from_axis_angle({1, 0, 0}, radians(5.0f));
    w.create(Name{"glider"}, Transform{.position = {-30.0f, 14.0f, 10.0f}, .rotation = pitch, .scale = {2.4f, 0.08f, 0.9f}},
             MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.95f, 0.95f, 0.9f, 1}, .roughness = 0.6f})},
             RigidBody{.desc = {.shape = ShapeDesc::box({1.2f, 0.04f, 0.45f}), .linear_velocity = {0, 0, -9.0f}, .mass = 4.0f, .linear_damping = 0.0f}},
             Airfoil{.area = 2.0f, .lift_slope = 6.0f}, AirDrag{.area = 0.2f, .drag = 0.3f});

    w.create(Transform{.position = {0, 12, 38}, .rotation = Quat::from_axis_angle({1, 0, 0}, -0.28f)}, Camera{.far_plane = 400.0f},
             fly::FlyCamera{.speed = 10.0f, .fast_speed = 30.0f, .pitch = -0.28f});
    ctx.sun = {.direction = {-0.45f, -0.8f, -0.4f}, .intensity = 1.5f};
    ctx.environment.exposure = 0.5f;
    ctx.log.info("physics playground: WASD/QE fly, right mouse look; F1 -> physics/fields overlays");
    ctx.debug_show.physics = overlays_;
    ctx.debug_show.fields = overlays_;
}

} // namespace physics_playground
