#pragma once

#include "Astral/Render/RenderTypes.hpp"

#include <glm/glm.hpp>
#include <span>
#include <vector>

namespace Astral {

struct TriangleMesh {
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
};

// Extracts the zero surface of an SDF shape list inside [min, max] with naive surface
// nets (one vertex per surface cell, quads across sign-changing edges). Shapes must be
// in evaluation order. Used to give SDF geometry physics collision.
TriangleMesh MeshSdfRegion(std::span<const Render::SdfShapeDesc> shapes, const glm::vec3& min, const glm::vec3& max,
                           float cellSize);

} // namespace Astral
