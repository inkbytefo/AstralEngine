#pragma once

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/core/reflect.hpp>
#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/debug_draw.hpp>
#include <astral/render/mesh.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/ui.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace astral {

class RenderGraph;
struct MeshTag;
struct MaterialTag;
struct ShaderTag;
struct RenderTextureTag;
/// GPU geometry owned by the Renderer (Renderer::create_mesh).
using MeshHandle = Handle<MeshTag>;
/// A surface description owned by the Renderer (Renderer::create_material). Shared by any number of
/// instances; editing it with update_material changes all of them.
using MaterialHandle = Handle<MaterialTag>;
/// An image the Renderer owns (Renderer::create_texture), used by materials. Not a gpu::TextureHandle.
using TextureHandle = Handle<RenderTextureTag>;
/// A compiled .ashader surface shader (Renderer::create_shader); materials point at it.
using ShaderHandle = Handle<ShaderTag>;

/// 8-bit RGBA pixels, rows top to bottom. The Renderer builds the mip chain.
/// Pixel layout of ImageData::levels. BC5 keeps two channels (normal map X/Y; Z is rebuilt in the
/// shader), BC7 four; both are 4x4 blocks of 16 bytes (see gpu::texture_data_size).
enum class ImageFormat : std::uint8_t { RGBA8, BC5, BC7 };

struct ImageData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba{}; ///< width * height * 4 bytes; mips are built on upload.
    bool srgb = true;               ///< Colour (base colour, emissive) = true; data (normals, roughness) = false.
    /// Ready-made mip chain (cooked assets), level 0 first, in `format`. When set, `rgba` is unused.
    ImageFormat format = ImageFormat::RGBA8;
    std::vector<std::vector<std::uint8_t>> levels{};
};

/// Box-filtered RGBA8 mip chain of an image (level 0 = `rgba`); colour images average in linear space.
std::vector<std::vector<std::uint8_t>> build_mips(const ImageData& image);

/// Metallic-roughness PBR, the glTF model. Texture values multiply the factors; invalid = none.
struct MaterialDesc {
    Vec4 base_color{1, 1, 1, 1};
    float roughness = 0.6f;
    float metallic = 0.0f;
    Vec3 emissive{};       ///< Emitted radiance (rgb). Lights the scene in the ray-traced path.
    TextureHandle base_color_texture{};
    TextureHandle metallic_roughness_texture{}; ///< glTF layout: G = roughness, B = metallic.
    TextureHandle normal_texture{};             ///< Tangent space; no vertex tangents needed.
    TextureHandle emissive_texture{};
    float normal_scale = 1.0f;
    /// A custom surface shader (ADR-0022). Invalid = the standard PBR shader with the values above.
    ShaderHandle shader{};
    /// The shader's uniform block (layout_uniforms words; numbers as float). Textures go in `textures`.
    std::vector<float> params{};
    struct ParamTexture {
        std::uint32_t offset = 0;   ///< Word of the sampler2D uniform.
        TextureHandle texture{};    ///< Invalid = white (or a flat normal when `normal`).
        bool normal = false;
    };
    std::vector<ParamTexture> textures{};
};

/// One mesh to draw this frame. An invalid material means Renderer::default_material().
struct MeshInstance {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 transform = Mat4::identity();
    /// Last frame's transform, for per-object motion vectors (TAA, denoiser). Unset = did not move.
    std::optional<Mat4> previous_transform = {};
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
    std::span<const MeshInstance> meshes = {};
    std::span<const SdfShape> sdf = {};
    std::span<const PointLight> lights = {};
    std::span<const RectLight> rect_lights = {};
    DirectionalLight sun = {};
    Environment environment = {};
    /// Drawn over the final image (see DebugDraw): hidden behind meshes unless `on_top`.
    std::span<const DebugLine> debug_lines = {};
    /// 2D overlay (debug UI, HUD, sprites), drawn last, in order.
    std::span<const UiLayer> ui = {};
    float time = 0.0f; ///< Seconds; TIME in .ashader code.
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
/// `Motion`: screen-space motion vectors (red = horizontal, green = vertical speed).
enum class DebugView : std::uint32_t { None, Albedo, Normals, DiffuseLight, Specular, Motion };

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

/// Hybrid deferred renderer built on RenderGraph.
///
/// Usage contract (the same for App and for engines/tools using the module directly):
///   * resources — meshes and materials — are created once and referenced by handle;
///   * every frame the caller *extracts* a RenderScene (plain spans, nothing retained) and calls render();
///   * settings can change at runtime (set_settings); the renderer resets what depends on them. Raster G-buffer (meshes + raymarched SDFs), then
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
    /// Replaces the geometry behind a handle (hot reload): instances keep drawing the same handle.
    bool update_mesh(MeshHandle mesh, const MeshData& data);
    void destroy(MeshHandle mesh);

    /// Uploads an image with a full mip chain (blocking). Invalid handle on bad data.
    TextureHandle create_texture(const ImageData& image);
    /// Pipelines for a compiled surface shader; invalid if the build failed. Destroy after the materials
    /// that use it (until then they draw with it).
    ShaderHandle create_shader(const SurfaceShaderBuild& build, std::string_view debug_name = {});
    void destroy(ShaderHandle shader);
    /// Why the last attempt to compile the custom surfaces into the ray-traced shaders failed ("" = fine).
    /// While it fails, custom materials show their base values in ray-traced reflections and GI.
    [[nodiscard]] std::string_view shader_error() const noexcept;
    /// Whether textures in `format` can be created (BC needs gpu::Feature::TextureCompressionBC).
    [[nodiscard]] bool supports(ImageFormat format) const noexcept;
    /// Replaces the image behind a handle (hot reload); materials using it see the new one.
    bool update_texture(TextureHandle texture, const ImageData& image);
    void destroy(TextureHandle texture);

    MaterialHandle create_material(const MaterialDesc& desc);
    /// False for a dead handle. Takes effect from the next render().
    bool update_material(MaterialHandle material, const MaterialDesc& desc);
    void destroy(MaterialHandle material);
    [[nodiscard]] const MaterialDesc* material(MaterialHandle material) const noexcept;
    /// Used by instances whose material is invalid or destroyed (white, roughness 0.6).
    [[nodiscard]] MaterialHandle default_material() const noexcept;

    /// Changes technique/quality at runtime. Turning ray tracing on/off, or changing GI or the
    /// denoiser, resets the temporal history.
    void set_settings(const RenderSettings& settings);
    [[nodiscard]] const RenderSettings& settings() const noexcept;

    /// Records the frame into `cmd`; `target` ends in ColorTarget state.
    /// Per-frame data is ring-buffered and synchronised with the GPU automatically.
    void render(gpu::CommandList& cmd, gpu::TextureHandle target, const CameraData& camera,
                const RenderScene& scene);

    [[nodiscard]] const RenderStats& stats() const noexcept;
    /// GPU time per render pass of a recent frame, and their total (see RenderGraph::gpu_timings).
    [[nodiscard]] std::span<const PassTiming> gpu_timings() const noexcept;
    [[nodiscard]] float gpu_frame_ms() const noexcept;
    /// True when frames use the ray-traced path (settings + device support).
    [[nodiscard]] bool ray_tracing() const noexcept;
    /// Forget temporal history (TAA, denoiser, GI) — call on camera cuts and teleports.
    void reset_history();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral

ASTRAL_REFLECT_ENUM(astral::SdfPrimitive, Sphere, Box, Torus, Capsule, Cylinder)
ASTRAL_REFLECT_ENUM(astral::SdfOperation, Union, Subtract, Intersect, SmoothUnion, SmoothSubtract)
ASTRAL_REFLECT(astral::SdfShape, primitive, operation, position, rotation, size, blend, color, roughness, metallic)
ASTRAL_REFLECT(astral::PointLight, position, color, intensity, radius, source_radius)
ASTRAL_REFLECT(astral::RectLight, position, right, up, color, intensity, two_sided)
ASTRAL_REFLECT(astral::DirectionalLight, direction, color, intensity, shadows, angular_radius)
ASTRAL_REFLECT(astral::Environment, sky, ground, ambient, exposure)
ASTRAL_REFLECT(astral::MaterialDesc, base_color, roughness, metallic, emissive, normal_scale)
ASTRAL_REFLECT_ENUM(astral::DebugView, None, Albedo, Normals, DiffuseLight, Specular, Motion)
ASTRAL_REFLECT(astral::GiSettings, probes_x, probes_y, probes_z, rays_per_probe, hysteresis, volume_min, volume_max)
ASTRAL_REFLECT(astral::RenderSettings, shadow_map_size, ray_tracing, gi, denoise_iterations, max_history, taa_blend,
               bloom_strength, max_ray_distance, near_gi_radius, debug_view)
