#pragma once

#include "gpu_layouts.hpp"

#include <astral/core/handle.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/mesh.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

class MeshStore {
public:
    explicit MeshStore(gpu::Device& device) noexcept : device_(device) {}
    ~MeshStore();
    MeshStore(const MeshStore&) = delete;
    MeshStore& operator=(const MeshStore&) = delete;

    MeshHandle create(const MeshData& data, bool build_blas);
    bool update(MeshHandle handle, const MeshData& data, bool build_blas);
    void destroy(MeshHandle handle);

    /// Builds acceleration structures for meshes created while ray tracing was off.
    void ensure_blas_all();

    [[nodiscard]] const GpuMesh* get(MeshHandle handle) const noexcept { return meshes_.get(handle); }

private:
    bool upload(const MeshData& data, bool build_blas, GpuMesh& mesh);

    gpu::Device& device_;
    HandlePool<GpuMesh, MeshTag> meshes_;
};

} // namespace astral
