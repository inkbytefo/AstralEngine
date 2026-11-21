#pragma once

#include "Astral/Render/RenderTypes.hpp"

namespace Astral::Render::Primitives {

// Simple procedural meshes (counter-clockwise front faces, tangents included).
MeshDesc Cube(float halfExtent = 0.5f);
MeshDesc Sphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);
MeshDesc Plane(float halfSize = 5.0f, float uvScale = 1.0f); // XZ plane facing +Y

} // namespace Astral::Render::Primitives
