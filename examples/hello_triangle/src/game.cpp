#include "game.hpp"

using namespace astral;

namespace hello_triangle {

MeshData triangle_mesh() {
    MeshData mesh;
    mesh.vertices = {
        {.position = {-1.0f, -0.8f, 0.0f}, .normal = {0, 0, 1}, .uv = {0.0f, 0.0f}},
        {.position = {1.0f, -0.8f, 0.0f}, .normal = {0, 0, 1}, .uv = {1.0f, 0.0f}},
        {.position = {0.0f, 0.9f, 0.0f}, .normal = {0, 0, 1}, .uv = {0.5f, 1.0f}},
    };
    mesh.indices = {0, 1, 2}; // counter-clockwise seen from +z
    return mesh;
}

void TriangleGame::on_start(Context& ctx) {
    const MeshHandle mesh = ctx.materials().create_mesh(triangle_mesh());
    const MaterialHandle material = ctx.materials().load_material("materials/triangle.material");
    ctx.world.create(Transform{}, MeshRenderer{mesh, material});
    ctx.world.create(Transform{.position = {0, 0, 3.2f}}, Camera{});
    ctx.environment.sky = {0.02f, 0.02f, 0.04f};
    ctx.environment.ground = {0.02f, 0.02f, 0.04f};
}

} // namespace hello_triangle
