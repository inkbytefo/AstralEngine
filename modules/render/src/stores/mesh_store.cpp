#include "stores/mesh_store.hpp"

#include <algorithm>
#include <cstdint>

namespace astral {

MeshStore::~MeshStore() {
    meshes_.for_each([&](MeshHandle, GpuMesh& mesh) {
        device_.destroy(mesh.vertices);
        device_.destroy(mesh.indices);
        device_.destroy(mesh.blas);
    });
}

bool MeshStore::upload(const MeshData& data, bool build_blas, GpuMesh& mesh) {
    if (data.vertices.empty() || data.indices.empty()) return false;
    // Shaders fetch vertices by index and BLAS builds trust them: every index must name a vertex.
    const std::size_t vertex_count = data.vertices.size();
    for (std::uint32_t index : data.indices) {
        if (index >= vertex_count) return false;
    }
    const std::uint64_t vbytes = data.vertices.size() * sizeof(Vertex);
    const std::uint64_t ibytes = data.indices.size() * sizeof(std::uint32_t);
    mesh.vertices = device_.create_buffer({.size = vbytes, .usage = gpu::BufferUsage::Storage});
    mesh.indices = device_.create_buffer({.size = ibytes, .usage = gpu::BufferUsage::Index});
    if (!device_.write_buffer(mesh.vertices, data.vertices.data(), vbytes) ||
        !device_.write_buffer(mesh.indices, data.indices.data(), ibytes)) {
        device_.destroy(mesh.vertices);
        device_.destroy(mesh.indices);
        return false;
    }
    mesh.index_count = static_cast<std::uint32_t>(data.indices.size());
    mesh.bounds_min = mesh.bounds_max = data.vertices[0].position;
    for (const Vertex& v : data.vertices) {
        mesh.bounds_min = {std::min(mesh.bounds_min.x, v.position.x), std::min(mesh.bounds_min.y, v.position.y),
                           std::min(mesh.bounds_min.z, v.position.z)};
        mesh.bounds_max = {std::max(mesh.bounds_max.x, v.position.x), std::max(mesh.bounds_max.y, v.position.y),
                           std::max(mesh.bounds_max.z, v.position.z)};
    }
    if (build_blas) {
        mesh.blas = device_.create_blas({.vertices = mesh.vertices,
                                         .vertex_count = static_cast<std::uint32_t>(data.vertices.size()),
                                         .vertex_stride = sizeof(Vertex),
                                         .indices = mesh.indices,
                                         .index_count = mesh.index_count,
                                         .debug_name = "astral.mesh_blas"});
    }
    return true;
}

MeshHandle MeshStore::create(const MeshData& data, bool build_blas) {
    GpuMesh mesh;
    if (!upload(data, build_blas, mesh)) return {};
    return meshes_.create(mesh);
}

bool MeshStore::update(MeshHandle handle, const MeshData& data, bool build_blas) {
    GpuMesh* mesh = meshes_.get(handle);
    if (!mesh) return false;
    GpuMesh fresh;
    if (!upload(data, build_blas, fresh)) return false;
    device_.destroy(mesh->vertices);
    device_.destroy(mesh->indices);
    device_.destroy(mesh->blas);
    *mesh = fresh;
    return true;
}

void MeshStore::destroy(MeshHandle handle) {
    if (const GpuMesh* mesh = meshes_.get(handle)) {
        device_.destroy(mesh->vertices);
        device_.destroy(mesh->indices);
        device_.destroy(mesh->blas);
        meshes_.destroy(handle);
    }
}

void MeshStore::ensure_blas_all() {
    meshes_.for_each([&](MeshHandle, GpuMesh& mesh) {
        if (mesh.blas.valid()) return;
        const auto vertex_count = static_cast<std::uint32_t>(device_.buffer_size(mesh.vertices) / sizeof(Vertex));
        mesh.blas = device_.create_blas({.vertices = mesh.vertices,
                                         .vertex_count = vertex_count,
                                         .vertex_stride = sizeof(Vertex),
                                         .indices = mesh.indices,
                                         .index_count = mesh.index_count,
                                         .debug_name = "astral.mesh_blas"});
    });
}

} // namespace astral
