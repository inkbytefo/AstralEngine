#include "Astral/Render/MeshPrimitives.hpp"

#include <cmath>

namespace Astral::Render::Primitives {

MeshDesc Cube(float h) {
    MeshDesc mesh;
    mesh.debugName = "cube";
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    for (const Face& f : faces) {
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        const glm::vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const glm::vec2& c : corners) {
            Vertex v;
            v.position = (f.n + f.u * c.x + f.v * c.y) * h;
            v.normal = f.n;
            v.tangent = glm::vec4(f.u, 1.0f);
            v.uv = glm::vec2(c.x * 0.5f + 0.5f, 0.5f - c.y * 0.5f);
            mesh.vertices.push_back(v);
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return mesh;
}

MeshDesc Sphere(float radius, uint32_t segments, uint32_t rings) {
    MeshDesc mesh;
    mesh.debugName = "sphere";
    const float pi = 3.14159265359f;
    for (uint32_t r = 0; r <= rings; ++r) {
        float v = float(r) / float(rings);
        float theta = v * pi;
        for (uint32_t s = 0; s <= segments; ++s) {
            float u = float(s) / float(segments);
            float phi = u * 2.0f * pi;
            glm::vec3 n(std::sin(theta) * std::cos(phi), std::cos(theta), -std::sin(theta) * std::sin(phi));
            Vertex vert;
            vert.position = n * radius;
            vert.normal = n;
            vert.tangent = glm::vec4(-std::sin(phi), 0.0f, -std::cos(phi), 1.0f);
            vert.uv = glm::vec2(u, v);
            mesh.vertices.push_back(vert);
        }
    }
    const uint32_t stride = segments + 1;
    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t a = r * stride + s, b = a + stride;
            mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return mesh;
}

MeshDesc Plane(float h, float uvScale) {
    MeshDesc mesh;
    mesh.debugName = "plane";
    const glm::vec2 corners[4] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};
    for (const glm::vec2& c : corners) {
        Vertex v;
        v.position = glm::vec3(c.x * h, 0.0f, c.y * h);
        v.normal = glm::vec3(0, 1, 0);
        v.tangent = glm::vec4(1, 0, 0, 1);
        v.uv = (c * 0.5f + 0.5f) * uvScale;
        mesh.vertices.push_back(v);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

} // namespace Astral::Render::Primitives
