#pragma once
#include <astral/astral.hpp>

namespace hello_triangle {

/// The mesh of the picture: one triangle in the XY plane facing +z, corners red / green / blue by texture coordinate.
[[nodiscard]] astral::MeshData triangle_mesh();

class TriangleGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace hello_triangle
