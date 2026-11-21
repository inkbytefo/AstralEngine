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

class RenderGraph;
struct MeshTag;
using MeshHandle = Handle<MeshTag>;

/// One mesh instance to draw this frame, with a simple PBR material.
struct DrawItem {
    MeshHandle mesh;
    Mat4 transform = Mat4::identity();
    Vec4 color{1, 1, 1, 1};
    float roughness = 0.6f;
    float metallic = 0.0f;
    float emissive = 0.0f; ///< Glow strength (multiplies color).
};

enum class SdfPrimitive : std::uint32_t { Sphere, Box, Torus, Capsule, Cylinder };
enum class SdfOperation : std::uint32_t { Union, Subtract, Intersect, SmoothUnion, SmoothSubtract };

/// A signed-distance-field shape. Shapes are combined in list order with their operation, so
/// smooth unions melt into each other and subtractions carve the shapes before them.
/// `size`: Sphere x=radius · Box xyz=half extents, w=rounding · Torus x=ring, y=tube ·
/// Capsule x=radius, y=half length · Cylinder x=radius, y=half height, w=rounding.
struct SdfShape {
    SdfPrimitive primitive = SdfPrimitive::Sphere;
    SdfOperation operation = SdfOperation::SmoothUnion;
    Vec3 position{};
    Quat rotation{};
    Vec4 size{0.5f, 0.5f, 0.5f, 0.0f};
    float blend = 0.3f; ///< Smoothing radius for the smooth operations.
    Vec4 color{1, 1, 1, 1};
    float roughness = 0.4f;
    float metallic = 0.0f;
};

struct PointLight {
    Vec3 position{};
    Vec3 color{1, 1, 1};
    float intensity = 10.0f;
    float radius = 10.0f; ///< No light beyond this distance.
};

struct CameraData {
    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    Vec3 position{};
};

/// The sun.
struct DirectionalLight {
    Vec3 direction{-0.4f, -1.0f, -0.3f}; ///< Direction the light travels.
    Vec3 color{1, 1, 1};
    float intensity = 3.0f;
    bool shadows = true;
};

struct Environment {
    Vec3 sky{0.45f, 0.6f, 0.85f};
    Vec3 ground{0.18f, 0.16f, 0.14f};
    float ambient = 0.35f; ///< Strength of the sky/ground light on surfaces.
    float exposure = 1.0f;
};

/// Everything to draw in one frame. Spans must stay valid until render() returns.
struct RenderScene {
    std::span<const DrawItem> meshes = {};
    std::span<const SdfShape> sdf = {};
    std::span<const PointLight> lights = {};
    DirectionalLight sun = {};
    Environment environment = {};
};

struct RenderSettings {
    std::uint32_t shadow_map_size = 2048;
};

struct RenderStats {
    std::uint32_t draw_calls = 0;        ///< G-buffer draws (one per distinct mesh).
    std::uint32_t instances = 0;
    std::uint32_t shadow_draw_calls = 0;
    std::uint32_t sdf_shapes = 0;
    std::uint32_t point_lights = 0;
    std::uint32_t passes = 0;            ///< Render graph passes executed.
};

/// Hybrid deferred renderer built on RenderGraph:
/// shadow map → mesh G-buffer → SDF raymarch → PBR lighting (mesh/SDF composite, sun shadows,
/// SDF soft shadows, point lights, sky ambient) → ACES tonemap into the target.
/// It knows nothing about the ECS; Astral::App fills RenderScene from components.
class Renderer {
public:
    Renderer(gpu::Device& device, gpu::Format target_format, const RenderSettings& settings = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Uploads a mesh (blocking). Invalid handle on failure, e.g. while a CommandList records.
    MeshHandle create_mesh(const MeshData& data);
    void destroy(MeshHandle mesh);

    /// Records the frame into `cmd`; `target` ends in ColorTarget state.
    /// Per-frame data is ring-buffered and synchronised with the GPU automatically.
    void render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                const RenderScene& scene);

    [[nodiscard]] const RenderStats& stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
