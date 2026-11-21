#pragma once

#include <astral/core/math.hpp>

#include <cstdint>
#include <vector>

namespace astral {

/// GPU vertex layout (scalar-packed, matches shaders/mesh.vert).
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices; ///< Counter-clockwise front faces.
};

/// Unit cube centred at the origin (side 1), flat normals per face.
MeshData make_cube();
/// Square on the XZ plane facing +Y.
MeshData make_plane(float size);
/// UV sphere of radius 0.5.
MeshData make_sphere(std::uint32_t segments, std::uint32_t rings);

} // namespace astral
