#include "Astral/Engine/Physics/SdfMeshing.hpp"
#include "Astral/Render/SdfMath.hpp"

#include <cmath>

namespace Astral {

TriangleMesh MeshSdfRegion(std::span<const Render::SdfShapeDesc> shapes, const glm::vec3& min, const glm::vec3& max,
                           float cellSize) {
    TriangleMesh mesh;
    if (shapes.empty() || cellSize <= 0.0f) return mesh;
    const glm::ivec3 n = glm::max(glm::ivec3(glm::ceil((max - min) / cellSize)), glm::ivec3(1));
    const glm::ivec3 points = n + 1;

    // Distance at every grid point.
    std::vector<float> field(size_t(points.x) * points.y * points.z);
    auto pointIndex = [&](int x, int y, int z) { return size_t(x) + size_t(points.x) * (size_t(y) + size_t(points.y) * z); };
    for (int z = 0; z < points.z; ++z)
        for (int y = 0; y < points.y; ++y)
            for (int x = 0; x < points.x; ++x)
                field[pointIndex(x, y, z)] = Render::Sdf::Evaluate(shapes, min + glm::vec3(x, y, z) * cellSize);

    // One vertex per cell that the surface crosses: the average of its edge crossings.
    std::vector<int32_t> cellVertex(size_t(n.x) * n.y * n.z, -1);
    auto cellIndex = [&](int x, int y, int z) { return size_t(x) + size_t(n.x) * (size_t(y) + size_t(n.y) * z); };
    static const int corner[8][3] = {{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
    static const int edges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (int z = 0; z < n.z; ++z) {
        for (int y = 0; y < n.y; ++y) {
            for (int x = 0; x < n.x; ++x) {
                float d[8];
                int inside = 0;
                for (int c = 0; c < 8; ++c) {
                    d[c] = field[pointIndex(x + corner[c][0], y + corner[c][1], z + corner[c][2])];
                    inside += d[c] < 0.0f;
                }
                if (inside == 0 || inside == 8) continue;
                glm::vec3 sum(0.0f);
                int count = 0;
                for (const auto& e : edges) {
                    float a = d[e[0]], b = d[e[1]];
                    if ((a < 0.0f) == (b < 0.0f)) continue;
                    float t = a / (a - b);
                    glm::vec3 pa(corner[e[0]][0], corner[e[0]][1], corner[e[0]][2]);
                    glm::vec3 pb(corner[e[1]][0], corner[e[1]][1], corner[e[1]][2]);
                    sum += glm::mix(pa, pb, t);
                    ++count;
                }
                cellVertex[cellIndex(x, y, z)] = int32_t(mesh.vertices.size());
                mesh.vertices.push_back(min + (glm::vec3(x, y, z) + sum / float(count)) * cellSize);
            }
        }
    }

    // A quad across every sign-changing grid edge, joining the 4 cells around it.
    auto quad = [&](int32_t a, int32_t b, int32_t c, int32_t d, bool flip) {
        if (a < 0 || b < 0 || c < 0 || d < 0) return;
        if (!flip) std::swap(b, d); // counter-clockwise seen from outside (Jolt front faces)
        mesh.indices.insert(mesh.indices.end(), {uint32_t(a), uint32_t(b), uint32_t(c), uint32_t(a), uint32_t(c), uint32_t(d)});
    };
    for (int z = 1; z < n.z; ++z) {
        for (int y = 1; y < n.y; ++y) {
            for (int x = 0; x < n.x; ++x) { // edges along X
                float a = field[pointIndex(x, y, z)], b = field[pointIndex(x + 1, y, z)];
                if ((a < 0.0f) != (b < 0.0f))
                    quad(cellVertex[cellIndex(x, y - 1, z - 1)], cellVertex[cellIndex(x, y, z - 1)],
                         cellVertex[cellIndex(x, y, z)], cellVertex[cellIndex(x, y - 1, z)], a < 0.0f);
            }
        }
    }
    for (int z = 1; z < n.z; ++z) {
        for (int y = 0; y < n.y; ++y) {
            for (int x = 1; x < n.x; ++x) { // edges along Y
                float a = field[pointIndex(x, y, z)], b = field[pointIndex(x, y + 1, z)];
                if ((a < 0.0f) != (b < 0.0f))
                    quad(cellVertex[cellIndex(x - 1, y, z - 1)], cellVertex[cellIndex(x - 1, y, z)],
                         cellVertex[cellIndex(x, y, z)], cellVertex[cellIndex(x, y, z - 1)], a < 0.0f);
            }
        }
    }
    for (int z = 0; z < n.z; ++z) {
        for (int y = 1; y < n.y; ++y) {
            for (int x = 1; x < n.x; ++x) { // edges along Z
                float a = field[pointIndex(x, y, z)], b = field[pointIndex(x, y, z + 1)];
                if ((a < 0.0f) != (b < 0.0f))
                    quad(cellVertex[cellIndex(x - 1, y - 1, z)], cellVertex[cellIndex(x, y - 1, z)],
                         cellVertex[cellIndex(x, y, z)], cellVertex[cellIndex(x - 1, y, z)], a < 0.0f);
            }
        }
    }
    return mesh;
}

} // namespace Astral
