#include <astral/render/mesh.hpp>

#include <numbers>

namespace astral {

MeshData make_cube() {
    MeshData mesh;
    struct Face {
        Vec3 normal, u, v;
    };
    const Face faces[6] = {
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
    };
    for (const Face& f : faces) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        const Vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const Vec2 c : corners) {
            const Vec3 p = f.normal * 0.5f + f.u * (c.x * 0.5f) + f.v * (c.y * 0.5f);
            mesh.vertices.push_back({p, f.normal, {(c.x + 1) * 0.5f, (1 - c.y) * 0.5f}});
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return mesh;
}

MeshData make_plane(float size) {
    const float h = size * 0.5f;
    MeshData mesh;
    mesh.vertices = {{{-h, 0, h}, {0, 1, 0}, {0, 1}},
                     {{h, 0, h}, {0, 1, 0}, {1, 1}},
                     {{h, 0, -h}, {0, 1, 0}, {1, 0}},
                     {{-h, 0, -h}, {0, 1, 0}, {0, 0}}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

MeshData make_sphere(std::uint32_t segments, std::uint32_t rings) {
    MeshData mesh;
    const float pi = std::numbers::pi_v<float>;
    for (std::uint32_t r = 0; r <= rings; ++r) {
        const float v = static_cast<float>(r) / static_cast<float>(rings);
        const float phi = v * pi;
        for (std::uint32_t s = 0; s <= segments; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(segments);
            const float theta = u * 2.0f * pi;
            const Vec3 n{std::sin(phi) * std::cos(theta), std::cos(phi), -std::sin(phi) * std::sin(theta)};
            mesh.vertices.push_back({n * 0.5f, n, {u, v}});
        }
    }
    const std::uint32_t stride = segments + 1;
    for (std::uint32_t r = 0; r < rings; ++r) {
        for (std::uint32_t s = 0; s < segments; ++s) {
            const std::uint32_t a = r * stride + s;
            const std::uint32_t b = a + stride;
            mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return mesh;
}

} // namespace astral
