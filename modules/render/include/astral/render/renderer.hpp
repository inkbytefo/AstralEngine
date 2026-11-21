#pragma once

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>
#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/mesh.hpp>

#include <cstdint>
#include <memory>
#include <span>

namespace astral {

struct MeshTag;
using MeshHandle = Handle<MeshTag>;

/// One object to draw this frame.
struct DrawItem {
    MeshHandle mesh;
    Mat4 transform = Mat4::identity();
    Vec4 color{1, 1, 1, 1};
};

struct CameraData {
    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    Vec3 position{};
};

struct DirectionalLight {
    Vec3 direction{-0.4f, -1.0f, -0.3f}; ///< Direction the light travels.
    Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float ambient = 0.15f;
};

struct RenderStats {
    std::uint32_t draw_calls = 0;
    std::uint32_t instances = 0;
};

/// Forward renderer: lit, depth-tested, instanced meshes. It knows nothing about the ECS —
/// it draws a list of DrawItems (Astral::App builds that list from the world).
/// The render graph, deferred lighting and SDF passes replace its internals in M3; this API
/// is what games and the App layer program against.
class Renderer {
public:
    Renderer(gpu::Device& device, gpu::Format color_format);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Uploads a mesh (blocking). Returns an invalid handle on failure, e.g. while a
    /// CommandList is being recorded.
    MeshHandle create_mesh(const MeshData& data);
    void destroy(MeshHandle mesh);

    /// Records the frame into `cmd`. `target` ends in ColorTarget state.
    /// Per-frame data lives in a small ring of buffers; a slot is reused only after the GPU
    /// finished the submission that last read it (several render() calls per frame are fine).
    void render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                std::span<const DrawItem> items, const DirectionalLight& light,
                Vec4 clear_color = {0.02f, 0.02f, 0.03f, 1.0f});

    [[nodiscard]] const RenderStats& stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
