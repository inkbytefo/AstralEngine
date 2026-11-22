#pragma once

#include <astral/core/math.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/surface_shader.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astral {

// --- CPU-side GPU resource records -------------------------------------------------------

struct GpuMesh {
    gpu::BufferHandle vertices;
    gpu::BufferHandle indices;
    gpu::AccelerationStructureHandle blas; // ray-traced path only
    std::uint32_t index_count = 0;
    Vec3 bounds_min{}, bounds_max{}; // local-space AABB
};

struct GpuTexture {
    gpu::TextureHandle texture;
    std::uint32_t width = 0, height = 0;
};

struct GpuShader {
    enum class Kind : std::uint8_t { Surface, Post, Sky } kind = Kind::Surface;
    gpu::PipelineHandle gbuffer, shadow;
    gpu::PipelineHandle forward; ///< Blended surfaces (kind Surface, blend) and the post / sky pipelines.
    bool blend = false;
    bool alpha_test = false;
    bool casts_shadows = true;
    std::string rt_chunk;                              ///< Ray-traced surface() (see compose_rt_surfaces)
    std::vector<SurfaceShaderBuild::Sampler> samplers; ///< Texture slots of the parameter block
};

// --- GPU layouts (scalar; mirror shaders/include/astral/scene.glsl) ----------------------

inline constexpr std::uint32_t no_texture = 0xFFFFFFFFu;
inline constexpr std::uint32_t no_shadow = 0xFFFFFFFFu;

struct GpuInstance {
    Mat4 model;
    Mat4 prev_model;
    std::uint32_t material = 0;
    std::uint32_t vertices[2] = {0, 0}; // 64-bit addresses as two words (4-byte aligned struct)
    std::uint32_t indices[2] = {0, 0};
};
static_assert(sizeof(GpuInstance) == 148);
static_assert(alignof(GpuInstance) == 4);
static_assert(offsetof(GpuInstance, prev_model) == 64);
static_assert(offsetof(GpuInstance, material) == 128);
static_assert(offsetof(GpuInstance, vertices) == 132);
static_assert(offsetof(GpuInstance, indices) == 140);

struct GpuMaterial {
    Vec4 base_color;
    Vec4 emissive_roughness;
    Vec4 params;                            // x metallic, y normal scale
    std::uint32_t textures[4];              // bindless: base colour, metallic-roughness, normal, emissive (~0 = none)
    std::uint32_t custom[4] = {0, 0, 0, 0}; // x: .ashader parameter offset (words), y: shader index + 1 (0 = standard)
};
static_assert(sizeof(GpuMaterial) == 80);
static_assert(offsetof(GpuMaterial, emissive_roughness) == 16);
static_assert(offsetof(GpuMaterial, params) == 32);
static_assert(offsetof(GpuMaterial, textures) == 48);
static_assert(offsetof(GpuMaterial, custom) == 64);

struct GpuFrame {
    Mat4 view_projection;
    Mat4 inv_view_projection;
    Mat4 light_view_projection;
    Vec4 camera_position;
    Vec4 sun_direction;
    Vec4 sun_color;
    Vec4 sky_color;
    Vec4 ground_color;
    std::uint32_t counts[4];
    Vec4 screen;
    Mat4 prev_view_projection;
    Vec4 ddgi_origin;
    Vec4 ddgi_spacing;
    std::uint32_t ddgi_counts[4];
    std::uint32_t rt_counts[4];
    Vec4 rt_params;
    Mat4 unjittered_view_projection;
    std::uint32_t custom[4]; // xy: .ashader parameter block address, z: TIME (float bits), w: encode_srgb
    Vec4 prev_camera_position;
};
static_assert(sizeof(GpuFrame) == 544);
static_assert(offsetof(GpuFrame, camera_position) == 192);
static_assert(offsetof(GpuFrame, counts) == 272);
static_assert(offsetof(GpuFrame, prev_view_projection) == 304);
static_assert(offsetof(GpuFrame, ddgi_origin) == 368);
static_assert(offsetof(GpuFrame, unjittered_view_projection) == 448);
static_assert(offsetof(GpuFrame, custom) == 512);
static_assert(offsetof(GpuFrame, prev_camera_position) == 528);

struct GpuPointLight {
    Vec4 position_radius;
    Vec4 color_intensity;
    Vec4 source;
};
static_assert(sizeof(GpuPointLight) == 48);
static_assert(offsetof(GpuPointLight, color_intensity) == 16);
static_assert(offsetof(GpuPointLight, source) == 32);

struct GpuRectLight {
    Vec4 position_two_sided;
    Vec4 right;
    Vec4 up;
    Vec4 radiance;
};
static_assert(sizeof(GpuRectLight) == 64);
static_assert(offsetof(GpuRectLight, right) == 16);
static_assert(offsetof(GpuRectLight, up) == 32);
static_assert(offsetof(GpuRectLight, radiance) == 48);

struct GpuSdfShape {
    Vec4 position_blend;
    Vec4 rotation;
    Vec4 size;
    Vec4 albedo_roughness;
    Vec4 params;
};
static_assert(sizeof(GpuSdfShape) == 80);
static_assert(offsetof(GpuSdfShape, rotation) == 16);
static_assert(offsetof(GpuSdfShape, size) == 32);
static_assert(offsetof(GpuSdfShape, albedo_roughness) == 48);
static_assert(offsetof(GpuSdfShape, params) == 64);

// --- Push-constant blocks (scalar; mirror shader push_constant blocks) -------------------

struct GBufferIds {
    std::uint32_t albedo, normal, emissive, depth, velocity, sdf_albedo, sdf_normal, sdf_depth, sdf_velocity;
};
static_assert(sizeof(GBufferIds) == 36);

struct RtLightingPush {
    std::uint64_t frame, tlas, instances, materials, lights, rects, shapes;
    GBufferIds g;
    std::uint32_t irr, dist, out_diffuse, out_specular, out_geometry;
    float near_gi_radius;
};
static_assert(sizeof(RtLightingPush) <= gpu::max_push_constant_size);
static_assert(offsetof(RtLightingPush, g) == 56);
static_assert(offsetof(RtLightingPush, irr) == 92);
static_assert(offsetof(RtLightingPush, near_gi_radius) == 112);

struct DdgiTracePush {
    std::uint64_t frame, tlas, instances, materials, lights, rects, shapes, rays;
    std::uint32_t irr_prev, dist_prev;
};
static_assert(sizeof(DdgiTracePush) <= gpu::max_push_constant_size);
static_assert(offsetof(DdgiTracePush, rays) == 56);
static_assert(offsetof(DdgiTracePush, irr_prev) == 64);

struct DdgiUpdatePush {
    std::uint64_t frame, rays;
    std::uint32_t prev, out, mode;
};
static_assert(sizeof(DdgiUpdatePush) <= gpu::max_push_constant_size);
static_assert(offsetof(DdgiUpdatePush, prev) == 16);

struct TemporalPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t geometry, prev_geometry, diffuse, specular, prev_diffuse, prev_specular, out_diffuse, out_specular;
    float max_history;
    std::uint32_t prev_moments, out_moments;
};
static_assert(sizeof(TemporalPush) <= gpu::max_push_constant_size);
static_assert(offsetof(TemporalPush, g) == 8);
static_assert(offsetof(TemporalPush, geometry) == 44);
static_assert(offsetof(TemporalPush, max_history) == 76);

struct AtrousPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t geometry, diffuse, specular, out_diffuse, out_specular;
    std::int32_t step;
    std::uint32_t moments;
};
static_assert(sizeof(AtrousPush) <= gpu::max_push_constant_size);
static_assert(offsetof(AtrousPush, g) == 8);
static_assert(offsetof(AtrousPush, step) == 64);

struct CompositePush {
    std::uint64_t frame, rects;
    GBufferIds g;
    std::uint32_t diffuse, specular, out_hdr, debug_view;
};
static_assert(sizeof(CompositePush) <= gpu::max_push_constant_size);
static_assert(offsetof(CompositePush, g) == 16);
static_assert(offsetof(CompositePush, diffuse) == 52);

struct TaaPush {
    std::uint64_t frame;
    GBufferIds g;
    std::uint32_t hdr, history, out_color;
    float blend;
};
static_assert(sizeof(TaaPush) <= gpu::max_push_constant_size);
static_assert(offsetof(TaaPush, g) == 8);
static_assert(offsetof(TaaPush, blend) == 56);

struct BloomDownPush {
    std::uint32_t src, dst;
    float texel[2];
    std::uint32_t size[2];
    std::uint32_t first;
};
static_assert(sizeof(BloomDownPush) == 28);
static_assert(sizeof(BloomDownPush) <= gpu::max_push_constant_size);

struct BloomUpPush {
    std::uint32_t low, current, dst;
    float texel[2];
    std::uint32_t size[2];
    float radius;
    float blend;
};
static_assert(sizeof(BloomUpPush) == 36);
static_assert(sizeof(BloomUpPush) <= gpu::max_push_constant_size);

struct MeshPush {
    std::uint64_t vertices, instances, frame, materials;
    std::uint32_t first_instance;
    std::uint64_t params; // .ashader parameter block (custom surface shaders)
    float time;
    std::uint64_t lights; // point lights (forward-lit blended surfaces)
};
static_assert(sizeof(MeshPush) == 64);
static_assert(sizeof(MeshPush) <= gpu::max_push_constant_size);
static_assert(offsetof(MeshPush, first_instance) == 32);
static_assert(offsetof(MeshPush, params) == 40);
static_assert(offsetof(MeshPush, time) == 48);
static_assert(offsetof(MeshPush, lights) == 56);

struct ScreenPush { // post and sky shaders
    std::uint64_t frame, materials;
    std::uint32_t material, scene;
};
static_assert(sizeof(ScreenPush) == 24);
static_assert(sizeof(ScreenPush) <= gpu::max_push_constant_size);
static_assert(offsetof(ScreenPush, material) == 16);

struct SdfPush {
    std::uint64_t frame, shapes;
    std::uint32_t mesh_depth, out_albedo, out_normal, out_depth, out_velocity;
};
static_assert(sizeof(SdfPush) <= gpu::max_push_constant_size);
static_assert(offsetof(SdfPush, mesh_depth) == 16);

struct LightingPush {
    std::uint64_t frame, lights, shapes;
    std::uint32_t albedo, normal, emissive, depth, sdf_albedo, sdf_normal, sdf_depth, out_hdr;
};
static_assert(sizeof(LightingPush) == 56);
static_assert(sizeof(LightingPush) <= gpu::max_push_constant_size);
static_assert(offsetof(LightingPush, albedo) == 24);

struct TonemapPush {
    std::uint32_t hdr;
    float exposure;
    std::uint32_t encode_srgb;
    std::uint32_t bloom = 0xFFFFFFFFu;
    float bloom_strength = 0.0f;
};
static_assert(sizeof(TonemapPush) == 20);
static_assert(sizeof(TonemapPush) <= gpu::max_push_constant_size);

struct UiPush {
    Vec2 scale, translate;
    std::uint64_t vertices, indices;
    std::int32_t vertex_offset;
    std::uint32_t texture, flags;
    float sdf_softness;
};
static_assert(sizeof(UiPush) == 48);
static_assert(sizeof(UiPush) <= gpu::max_push_constant_size);
static_assert(offsetof(UiPush, vertices) == 16);
static_assert(offsetof(UiPush, vertex_offset) == 32);

struct DebugLinePush {
    Mat4 view_projection;
    std::uint64_t vertices;
};
static_assert(sizeof(DebugLinePush) == 72);
static_assert(sizeof(DebugLinePush) <= gpu::max_push_constant_size);
static_assert(offsetof(DebugLinePush, vertices) == 64);

} // namespace astral
