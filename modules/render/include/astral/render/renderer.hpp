#pragma once

#include <astral/core/handle.hpp>
#include <astral/core/math.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/core/reflect.hpp>
#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/debug_draw.hpp>
#include <astral/render/image.hpp>
#include <astral/render/mesh.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/ui.hpp>

#include <array>
#include <cstdint>
#include <functional>
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

class JobSystem;

/// Box-filtered RGBA8 mip chain of an image (level 0 = `rgba`); colour images average in linear space. With `jobs`
/// the big levels are filtered concurrently; the bytes are the same either way.
std::vector<std::vector<std::uint8_t>> build_mips(const ImageData& image, JobSystem* jobs = nullptr);

/// Optional clearcoat dielectric top layer (M13, IOR 1.5).
struct ClearcoatDesc {
    float strength = 0.0f;
    float roughness = 0.1f;
};
/// Optional grazing microfiber sheen layer for cloth/velvet (Charlie NDF).
struct SheenDesc {
    Vec3 color{};
    float roughness = 0.5f;
};
/// Optional anisotropic GGX microfacet directionality (brushed metal/hair).
struct AnisotropyDesc {
    float strength = 0.0f; ///< [-1, 1]: stretches highlight along tangent (+) or bitangent (-).
    float rotation = 0.0f; ///< Radians around surface normal.
};
/// Optional dielectric transmission + Beer-Lambert volume absorption (glass/liquid).
struct TransmissionDesc {
    float strength = 0.0f;
    float ior = 1.5f;
    float thickness = 0.1f;
    Vec3 attenuation_color{1, 1, 1};
    float attenuation_distance = 1.0f;
};
/// Optional subsurface scattering parameters (wax/skin/foliage).
struct SubsurfaceDesc {
    float strength = 0.0f;
    Vec3 color{1.0f, 0.25f, 0.15f};
    float radius = 0.5f;
};
/// Optional high-frequency detail maps and Parallax Occlusion Mapping (POM).
struct DetailDesc {
    TextureHandle albedo_texture{};
    TextureHandle normal_texture{};
    TextureHandle height_texture{};
    float uv_scale = 4.0f;
    float normal_scale = 0.5f;
    float parallax_scale = 0.0f; ///< >0 enables POM displacement along view ray.
};

enum class TextureProjection : std::uint8_t { MeshUV, PlanarXY, PlanarXZ, PlanarYZ, Triplanar };

struct TextureMapping {
    TextureProjection projection = TextureProjection::MeshUV;
    Vec2 uv_scale{1, 1};
    Vec2 uv_offset{};
    float uv_rotation = 0.0f; ///< Radians.
    float tile_size_m = 1.0f;
    float blend_sharpness = 4.0f;
    bool world_space = true;
};

/// Metallic-roughness PBR + M13 advanced shading layers. Texture values multiply the factors; invalid = none.
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
    ShadingModel shading_model = ShadingModel::Standard;
    ClearcoatDesc clearcoat{};
    SheenDesc sheen{};
    AnisotropyDesc anisotropy{};
    TransmissionDesc transmission{};
    SubsurfaceDesc subsurface{};
    DetailDesc detail{};
    TextureMapping mapping{};
    float wetness = 0.0f; ///< [0, 1]: darkens porous surfaces, smooths roughness, adds water clearcoat.
    /// A custom surface shader (ADR-0022). Invalid = the standard PBR shader with the values above.
    ShaderHandle shader{};
    /// The shader's uniform block (layout_uniforms words, typed per ADR-0042: float bits, int32, bool 0/1). Textures go in `textures`.
    std::vector<std::uint32_t> params{};
    struct ParamTexture {
        std::uint32_t offset = 0;   ///< Word of the sampler2D uniform.
        TextureHandle texture{};    ///< Invalid = white (or a flat normal when `normal`).
        bool normal = false;
    };
    std::vector<ParamTexture> textures{};
};

/// A box-projected deferred decal (paint, impact marks, wet puddles) applied onto meshes and SDFs.
struct Decal {
    Mat4 transform = Mat4::identity(); ///< Oriented unit box [-0.5, 0.5]^3; projects along local -Y.
    Vec4 color{1, 1, 1, 1};            ///< RGB tint, A opacity.
    TextureHandle albedo_texture{};
    TextureHandle normal_texture{};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float wetness = 0.0f;              ///< Adds wetness clearcoat/darkening onto underlying surfaces.
    float normal_strength = 0.0f;
    bool affect_roughness = false;
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
    float aerial_extinction = 0.0f; ///< Horizontal extinction per metre; zero disables perspective.
    float aerial_start_m = 0.0f;   ///< Clear distance before aerial perspective begins.
    float fog_extinction = 0.0f;   ///< Froxel fog extinction per metre; zero disables fog.
    float fog_range_m = 100.0f;    ///< Maximum fog integration distance.
};

/// A local participating medium. Occupancy is binary, laid out as (z * side + y) * side + x.
struct SmokeVolumeView {
    Vec3 centre{};
    float cell_size_m = 0.25f;
    float extinction_per_m = 0.0f;
    std::uint32_t side = 0;
    std::span<const std::uint8_t> occupancy{}; ///< Empty with a volume material lets the shader fill the entire grid.
    MaterialHandle material{};
    float age = 0;
};

/// An analytic burst evaluated per particle on the GPU.
struct ParticleBurst {
    Vec3 position{};
    float age = 0;
    Vec3 drift{};
    float duration = 1;
    Vec3 color{1, 0.55f, 0.2f};
    float size_m = 0.04f;
    std::uint32_t count = 64;
    std::uint32_t seed = 1;
    MaterialHandle material{};
    float soft_distance_m = 0; ///< Fade as the billboard approaches the nearest solid surface.
    std::uint64_t simulation_id = 0; ///< Stable nonzero ID enables persistent GPU simulation. Unique per scene.
    float restitution = 0.45f; ///< 0 stops normal motion, 1 preserves it at a depth collision.
    float lighting = 0.0f; ///< Amount of ambient/DDGI modulation. Zero keeps the authored emissive color.
};

/// Everything to draw in one frame. Spans must stay valid until render() returns.
struct RenderScene {
    std::span<const MeshInstance> meshes = {};
    std::span<const SdfShape> sdf = {};
    std::span<const PointLight> lights = {};
    std::span<const RectLight> rect_lights = {};
    std::span<const Decal> decals = {};
    DirectionalLight sun = {};
    Environment environment = {};
    /// Drawn over the final image (see DebugDraw): hidden behind meshes unless `on_top`.
    std::span<const DebugLine> debug_lines = {};
    /// 2D overlay (debug UI, HUD, sprites), drawn last, in order.
    std::span<const UiLayer> ui = {};
    float time = 0.0f; ///< Seconds; TIME in .ashader code.
    /// A material whose shader is shader_type sky: replaces the background (not the lighting).
    MaterialHandle sky{};
    /// Materials with shader_type post shaders, applied in order after tonemapping (before debug lines and UI).
    std::span<const MaterialHandle> post = {};
    std::array<double, 3> world_origin{}; ///< Absolute offset of the current floating render origin, in meters.
    std::span<const SmokeVolumeView> smoke = {};
    std::span<const ParticleBurst> particles = {};
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
    /// À-trous passes (0 = temporal only); at most max_denoise_iterations (larger values are clamped).
    std::uint32_t denoise_iterations = 3;
    static constexpr std::uint32_t max_denoise_iterations = 5; ///< Widest step 16 px; more only blurs and costs.
    float max_history = 32.0f;            ///< Temporal accumulation length (frames).
    float taa_blend = 0.1f;               ///< Weight of the new frame in TAA.
    float bloom_strength = 0.04f;
    float max_ray_distance = 1000.0f;
    /// Near-field ray-traced GI range: within it, one ray per pixel gathers the actual bounce light
    /// (contact shadows + colour bleeding); beyond it the probes take over. 0 = probes only.
    float near_gi_radius = 0.75f;
    /// Spatiotemporal ReSTIR DI for multi-light scenes (> 2 local point/rect lights). False = single-frame RIS.
    bool restir_di = true;
    /// Unbiased progressive multi-bounce path tracer reference mode (M13, requires RT hardware).
    bool path_tracer = false;
    std::uint32_t path_tracer_max_bounces = 6;
    DebugView debug_view = DebugView::None;
    bool fog_self_shadows = true; ///< Local media attenuate direct sun light within their own volume.
    bool fog_ddgi = true; ///< Sample diffuse probe lighting in fog on the hybrid path.
    bool sky_ibl = false; ///< Use the active sky material as the HDR lighting environment.
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
    std::uint32_t decals = 0;
    std::uint32_t path_tracer_samples = 0;
    bool temporal_history_used = false; ///< Hybrid RT history was available to the temporal passes.
    std::uint32_t volume_bricks = 0; ///< Resident 8-cube pages in the shared volume atlas.
    std::uint32_t volume_brick_fallbacks = 0; ///< Volumes that exceeded the page budget.
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
    /// Creates an offscreen color render target that can be rendered into with render() and sampled in UiLayer.
    TextureHandle create_render_target(std::uint32_t width, std::uint32_t height);
    /// Resizes an offscreen render target (old GPU image is deferred-destroyed safely after in-flight frames).
    bool resize_render_target(TextureHandle target, std::uint32_t width, std::uint32_t height);
    /// Returns the underlying gpu::TextureHandle of a render target (for render() / layout transitions).
    [[nodiscard]] gpu::TextureHandle native_target(TextureHandle target) const noexcept;
    [[nodiscard]] gpu::Format target_format() const noexcept;
    /// Pipelines for a compiled surface shader; invalid if the build failed. Destroy after the materials
    /// that use it (until then they draw with it).
    ShaderHandle create_shader(const SurfaceShaderBuild& build, std::string_view debug_name = {});
    /// A post or sky shader (shader_type post / sky); a material points at it, RenderScene::post / sky use it.
    ShaderHandle create_shader(const ScreenShaderBuild& build, std::string_view debug_name = {});
    void destroy(ShaderHandle shader);
    /// The SPIR-V cache and job system that shader compiles use (borrowed: the owner keeps them alive, normally the
    /// App; null = compile serially, no cache). The ray-traced variants recompile through them whenever custom
    /// surface shaders change, and loaders that compile shaders for this Renderer pass them on.
    void set_shader_compile_services(const ShaderCompileServices& services) noexcept;
    /// Workers for CPU-heavy resource work (mip chains of uploaded textures). Borrowed; null = on the calling thread.
    void set_job_system(JobSystem* jobs) noexcept;
    [[nodiscard]] const ShaderCompileServices& shader_compile_services() const noexcept;
    /// Called (on the creating thread) after every texture, mesh, material or shader this Renderer creates or
    /// updates, so a long load can keep its window answering the OS. Keep it cheap; empty = none.
    void set_busy_callback(std::function<void()> callback);
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
    /// Clears `target` and records only 2D `ui` layers into `cmd`; `target` ends in ColorTarget state.
    /// Ideal when the 3D scene was rendered into an offscreen render target sampled inside the UI.
    void render_ui(gpu::CommandList& cmd, gpu::TextureHandle target, std::span<const UiLayer> ui,
                   Vec4 clear_color = {0.06f, 0.06f, 0.07f, 1.0f});

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
ASTRAL_REFLECT(astral::Environment, sky, ground, ambient, exposure, aerial_extinction, aerial_start_m, fog_extinction, fog_range_m)
ASTRAL_REFLECT_ENUM(astral::ShadingModel, Standard, Clearcoat, Sheen, Anisotropic, Transmission, Subsurface)
ASTRAL_REFLECT(astral::ClearcoatDesc, strength, roughness)
ASTRAL_REFLECT(astral::SheenDesc, color, roughness)
ASTRAL_REFLECT(astral::AnisotropyDesc, strength, rotation)
ASTRAL_REFLECT(astral::TransmissionDesc, strength, ior, thickness, attenuation_color, attenuation_distance)
ASTRAL_REFLECT(astral::SubsurfaceDesc, strength, color, radius)
ASTRAL_REFLECT(astral::DetailDesc, uv_scale, normal_scale, parallax_scale)
ASTRAL_REFLECT_ENUM(astral::TextureProjection, MeshUV, PlanarXY, PlanarXZ, PlanarYZ, Triplanar)
ASTRAL_REFLECT(astral::TextureMapping, projection, uv_scale, uv_offset, uv_rotation, tile_size_m, blend_sharpness, world_space)
ASTRAL_REFLECT(astral::MaterialDesc, base_color, roughness, metallic, emissive, normal_scale, shading_model, clearcoat,
               sheen, anisotropy, transmission, subsurface, detail, wetness, mapping)
ASTRAL_REFLECT(astral::Decal, transform, color, roughness, metallic, wetness, normal_strength, affect_roughness)
ASTRAL_REFLECT_ENUM(astral::DebugView, None, Albedo, Normals, DiffuseLight, Specular, Motion)
ASTRAL_REFLECT(astral::GiSettings, probes_x, probes_y, probes_z, rays_per_probe, hysteresis, volume_min, volume_max)
ASTRAL_REFLECT(astral::RenderSettings, shadow_map_size, ray_tracing, gi, denoise_iterations, max_history, taa_blend,
               bloom_strength, max_ray_distance, near_gi_radius, restir_di, path_tracer, path_tracer_max_bounces,
               debug_view, fog_self_shadows, fog_ddgi, sky_ibl)
