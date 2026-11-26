// The ground of the showcase: one height field for physics and the same heights as a render mesh, the lake (a WaterBody
// with a see-through surface) and the global, turning wind.
#include "stations/stations.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace astral;

namespace physics_showcase {

namespace {

constexpr std::uint32_t grid = 80;     // height samples per side
constexpr float cell = 1.5f;           // metres between samples: the terrain is 118.5 m wide
constexpr float half_size = 0.5f * static_cast<float>(grid - 1) * cell;

float smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

MeshData terrain_mesh(const std::vector<float>& heights) {
    MeshData mesh;
    mesh.vertices.reserve(std::size_t{grid} * grid);
    mesh.indices.reserve(std::size_t{grid - 1} * (grid - 1) * 6);
    const auto h = [&](std::uint32_t xi, std::uint32_t zi) { return heights[std::min(zi, grid - 1) * grid + std::min(xi, grid - 1)]; };
    for (std::uint32_t z = 0; z < grid; ++z) {
        for (std::uint32_t x = 0; x < grid; ++x) {
            const float px = static_cast<float>(x) * cell - half_size, pz = static_cast<float>(z) * cell - half_size;
            const Vec3 normal = normalize(Vec3{h(x == 0 ? 0 : x - 1, z) - h(x + 1, z), 2.0f * cell, h(x, z == 0 ? 0 : z - 1) - h(x, z + 1)});
            mesh.vertices.push_back({{px, heights[z * grid + x], pz}, normal, {static_cast<float>(x) * 0.25f, static_cast<float>(z) * 0.25f}});
        }
    }
    for (std::uint32_t z = 0; z + 1 < grid; ++z) {
        for (std::uint32_t x = 0; x + 1 < grid; ++x) {
            const std::uint32_t i = z * grid + x;
            mesh.indices.insert(mesh.indices.end(), {i, i + grid, i + 1, i + 1, i + grid, i + grid + 1}); // CCW seen from above
        }
    }
    return mesh;
}

} // namespace

float terrain_height(float x, float z) {
    const float r = std::sqrt(x * x + z * z);
    // Ripples on the slopes only: the lake bed and the plateau (r >= 28) stay flat.
    const float ripple = 0.4f * std::sin(x * 0.35f) * std::cos(z * 0.3f) * smooth(8.0f, 20.0f, r) * (1.0f - smooth(22.0f, 28.0f, r));
    return -2.5f + (plateau_height + 2.5f) * smooth(10.0f, 28.0f, r) + ripple;
}

void build_terrain_and_lake(Context& ctx) {
    World& w = ctx.world;
    std::vector<float> heights(std::size_t{grid} * grid);
    for (std::uint32_t z = 0; z < grid; ++z) {
        for (std::uint32_t x = 0; x < grid; ++x) {
            heights[z * grid + x] = terrain_height(static_cast<float>(x) * cell - half_size, static_cast<float>(z) * cell - half_size);
        }
    }
    const MeshHandle terrain = ctx.materials().create_mesh(terrain_mesh(heights));
    const MaterialHandle grass = ctx.materials().create_material({.base_color = {0.16f, 0.27f, 0.1f, 1}, .roughness = 0.92f});
    w.create(Name{"terrain"}, Transform{}, MeshRenderer{terrain, grass},
             RigidBody{.desc = {.shape = ShapeDesc::height_field(heights, grid, cell), .motion = MotionType::Static, .friction = 0.8f, .surface = 1}});

    // The plateau goes on past the height field: four flat strips around it (to the horizon), so the world has no edge
    // to see or to fall off.
    constexpr float outer = 400.0f;
    const float strip = outer - half_size, mid = 0.5f * (outer + half_size);
    const auto ground = [&](Vec3 centre, float sx, float sz) {
        w.create(Transform{.position = centre, .scale = {sx, 1, sz}}, MeshRenderer{ctx.meshes.plane, grass});
        w.create(Transform{.position = centre - Vec3{0, 0.5f, 0}},
                 RigidBody{.desc = {.shape = ShapeDesc::box({0.5f * sx, 0.5f, 0.5f * sz}), .motion = MotionType::Static, .friction = 0.8f, .surface = 1}});
    };
    // The render planes lie at the plateau height; the colliders are 1 m thick boxes with their tops there.
    ground({mid, plateau_height, 0}, strip, 2 * outer);
    ground({-mid, plateau_height, 0}, strip, 2 * outer);
    ground({0, plateau_height, mid}, 2 * half_size, strip);
    ground({0, plateau_height, -mid}, 2 * half_size, strip);

    // The lake: a water volume over the basin (its top is y = 0) and a see-through surface.
    w.create(Name{"lake"}, Transform{.position = {0, -3, 0}},
             WaterBody{.half_extents = {26, 3, 26}, .density = 1000.0f, .flow = {0.25f, 0, 0.1f}, .wave_height = 0.12f, .wave_length = 7.0f, .wave_speed = 1.2f});
    w.create(Transform{.position = {0, 0, 0}, .scale = {52, 1, 52}}, PrimitiveMesh{.shape = Primitive::Plane, .material_file = "materials/water.material"});

    // Wind: gusty and turning (the rule is in playground.luau).
    w.create(Name{"wind"}, Transform{}, WindZone{.velocity = {6, 0, 0}, .gust_strength = 3.0f, .gust_frequency = 0.3f, .turbulence = 1.0f, .global = true});
}

} // namespace physics_showcase
