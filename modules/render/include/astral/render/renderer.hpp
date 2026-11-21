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
    float radius = 10.0f;         ///< No light beyond this distance.
    float source_radius = 0.05f;  ///< Bulb size: softens ray-traced shadows.
};

/// A rectangular area light (a glowing panel: ceiling light, window, screen). Emits from the side
/// its normal `cross(right, up)` faces (or both). Ray-traced path only; it is visible to the
/// camera and in reflections, and casts physically soft shadows.
struct RectLight {
    Vec3 position{};
    Vec3 right{0.5f, 0, 0}; ///< Half extent along the first axis.
    Vec3 up{0, 0, 0.5f};    ///< Half extent along the second axis.
    Vec3 color{1, 1, 1};
    float intensity = 10.0f; ///< Emitted radiance = color * intensity.
    bool two_sided = false;
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
    float angular_radius = 0.0047f; ///< Radians; the real sun is ~0.0047. Larger = softer shadows.
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
    std::span<const RectLight> rect_lights = {};
    DirectionalLight sun = {};
    Environment environment = {};
};

/// Dynamic diffuse global illumination: a grid of probes that trace rays every frame.
struct GiSettings {
    std::uint32_t probes_x = 8, probes_y = 8, probes_z = 8;
    std::uint32_t rays_per_probe = 128;
    float hysteresis = 0.97f; ///< History weight: higher = smoother but slower to react.
    /// Probe volume. Equal min/max = fit around every mesh automatically.
    Vec3 volume_min{}, volume_max{};
};

/// What the ray-traced path shows (for tuning and debugging).
enum class DebugView : std::uint32_t { None, Albedo, Normals, DiffuseLight, Specular };

struct RenderSettings {
    std::uint32_t shadow_map_size = 2048; ///< Raster path.
    /// Ray-traced lighting (RT shadows, area lights, DDGI, reflections, denoiser, TAA, bloom)
    /// when the device supports it; otherwise the raster path.
    bool ray_tracing = true;
    GiSettings gi = {};
    std::uint32_t denoise_iterations = 3; ///< À-trous passes (0 = temporal only).
    float max_history = 32.0f;            ///< Temporal accumulation length (frames).
    float taa_blend = 0.1f;               ///< Weight of the new frame in TAA.
    float bloom_strength = 0.04f;
    float max_ray_distance = 1000.0f;
    /// Near-field ray-traced GI range: within it, one ray per pixel gathers the actual bounce light
    /// (contact shadows + colour bleeding); beyond it the probes take over. 0 = probes only.
    float near_gi_radius = 0.75f;
    DebugView debug_view = DebugView::None;
};

struct RenderStats {
    std::uint32_t draw_calls = 0;        ///< G-buffer draws (one per distinct mesh).
    std::uint32_t instances = 0;
    std::uint32_t shadow_draw_calls = 0;
    std::uint32_t sdf_shapes = 0;
    std::uint32_t point_lights = 0;
    std::uint32_t passes = 0;            ///< Render graph passes executed.
    bool ray_traced = false;             ///< This frame used the ray-traced path.
    std::uint32_t gi_probes = 0;
    std::uint32_t rect_lights = 0;
};

/// Hybrid deferred renderer built on RenderGraph. Raster G-buffer (meshes + raymarched SDFs), then
/// either the ray-traced path (ADR-0011): RT direct light + DDGI + RT reflections → SVGF-style
/// denoiser → composite → TAA → bloom → ACES; or the raster path: shadow map + PBR → ACES.
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
    /// True when frames use the ray-traced path (settings + device support).
    [[nodiscard]] bool ray_tracing() const noexcept;
    /// Forget temporal history (TAA, denoiser, GI) — call on camera cuts and teleports.
    void reset_history();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
