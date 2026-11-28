// Shared scene data layouts. Mirrors the GPU structs in src/gpu_layouts.hpp (scalar layout).
#ifndef ASTRAL_SCENE_GLSL
#define ASTRAL_SCENE_GLSL
#include <astral/bindless.glsl>

struct Vertex { vec3 position; vec3 normal; vec2 uv; vec2 uv1; };
struct Instance {
    mat4 model;
    mat4 prev_model;   // last frame's transform (motion vectors)
    uint material;     // index into the material table
    uvec2 vertices;    // Vertices address as 2 x uint32 (keeps the struct 4-byte aligned)
    uvec2 indices;     // uint32 triangle indices address
};
struct Material {
    vec4 base_color;
    vec4 emissive_roughness; // rgb emitted radiance, a roughness
    vec4 params;             // x metallic, y normal scale, z wetness, w unused
    uvec4 textures;          // bindless: base colour, metallic-roughness (G rough, B metal), normal, emissive; ~0u none
    uvec4 custom;            // x: .ashader parameter offset (words), y: custom shader key (0=standard), z: shading_model, w: flags
    vec4 coat_aniso;         // x clearcoat, y clearcoat_roughness, z anisotropy, w anisotropy_rotation
    vec4 sheen;              // rgb sheen_color, a sheen_roughness
    vec4 transmission;       // x strength, y ior, z thickness, w attenuation_distance
    vec4 subsurface;         // rgb subsurface_color, a subsurface_strength
    vec4 attenuation;        // rgb attenuation_color, a subsurface_radius
    uvec4 detail_textures;   // bindless: x detail_albedo, y detail_normal, z height_map, w occlusion
    vec4 detail_params;      // x uv_scale, y normal_scale, z parallax_scale, w occlusion_strength
    vec4 uv_transform;
    vec4 projection;
    vec4 projection_phase;
};
struct Decal {
    mat4 inv_transform;      // world -> decal local [-0.5, 0.5]^3
    vec4 color;              // rgb tint, a opacity
    vec4 params;             // x roughness, y metallic, z wetness, w normal_strength
    vec4 axis_up;            // xyz world projection normal (+Y), w affect_roughness
    uvec4 textures;          // x albedo, y normal, zw unused
};
const uint NO_TEXTURE = 0xFFFFFFFFu;
struct Frame {
    mat4 view_projection;
    mat4 inv_view_projection;
    mat4 light_view_projection;
    vec4 camera_position;
    vec4 sun_direction;   // xyz: direction of travel, w: raster shadow normal offset
    vec4 sun_color;       // rgb * intensity, w: 1 = sun casts shadows (ray-traced path)
    vec4 sky_color;       // rgb, a: ambient strength
    vec4 ground_color;    // rgb, a: exposure
    uvec4 counts;         // x: point lights, y: sdf shapes, z: shadow map bindless index (or ~0u), w: restir flags (bit0=active, bit1=history)
    vec4 screen;          // width, height, 1/width, 1/height
    // --- ray-traced path ---
    mat4 prev_view_projection; // previous frame, unjittered (reprojection)
    vec4 ddgi_origin;     // xyz first probe position, w hysteresis
    vec4 ddgi_spacing;    // xyz probe spacing, w unused
    uvec4 ddgi_counts;    // xyz probes per axis, w rays per probe
    uvec4 rt_counts;      // x rect lights, y frame index, z history valid, w ddgi history valid
    vec4 rt_params;       // x sun cone half-angle (rad), yz jitter (pixels), w max ray distance
    mat4 unjittered_view_projection; // this frame without TAA jitter (motion vectors)
    uvec4 custom;         // xy: .ashader parameter block address, z: TIME (float bits), w: encode_srgb
    vec4 prev_camera_position; // previous frame world camera position (reprojection depth validation)
    mat4 inv_prev_view_projection; // inverse of prev_view_projection (exact prev-frame world reconstruction)
    uint64_t scene_tlas;
    uint64_t scene_instances;
    uint64_t scene_materials;
    uint64_t scene_lights;
    uint64_t scene_rects;
    uint64_t scene_shapes;
    uint64_t scene_decals;
    uint decal_count;
    uint pt_params;       // bits 0..15: pt_sample_index, bits 16..23: max_bounces
    vec4 aerial;          // x extinction per metre, y start distance in metres
    vec4 fog;             // x extinction per metre, y maximum range
    uvec4 ibl;            // radiance map, filtered atlas, active, reserved
};
struct PointLight { vec4 position_radius; vec4 color_intensity; vec4 source; /* x source radius */ };
struct SdfShape {
    vec4 position_blend;   // xyz position, w smoothing radius
    vec4 rotation;         // quaternion (object -> world)
    vec4 size;             // primitive parameters
    vec4 albedo_roughness;
    vec4 params;           // x metallic, y operation, z primitive
};

ASTRAL_BUFFER(Vertices, Vertex);
ASTRAL_BUFFER(Instances, Instance);
ASTRAL_BUFFER(Materials, Material);
ASTRAL_BUFFER(PointLights, PointLight);
ASTRAL_BUFFER(SdfShapes, SdfShape);
ASTRAL_BUFFER(Decals, Decal);
layout(buffer_reference, scalar, buffer_reference_align = 8) buffer FrameRef { Frame f; };

const float PI = 3.14159265359;

uint64_t address_of(uvec2 a) { return uint64_t(a.x) | (uint64_t(a.y) << 32); }

vec3 quat_rotate(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

// Signed cofactor matrix of the upper 3x3: parallel to transpose(inverse(mat3(model))) for all
// invertible matrices (including non-uniform and negative scales), with zero divisions and no NaN on singular transforms.
mat3 normal_matrix_of(mat4 model) {
    mat3 m = mat3(model);
    vec3 c0 = cross(m[1], m[2]);
    vec3 c1 = cross(m[2], m[0]);
    vec3 c2 = cross(m[0], m[1]);
    float s = dot(m[0], c0) < 0.0 ? -1.0 : 1.0;
    return mat3(c0 * s, c1 * s, c2 * s);
}

// Screen-space motion vector (uv_now - uv_prev). If the previous clip position was behind the camera
// (prev.w <= 0), return a finite off-screen sentinel instead of overflowing RG16Float to +/-Inf.
vec2 compute_velocity(vec4 clip_now, vec4 clip_prev) {
    if (clip_prev.w <= 1e-5 || clip_now.w <= 1e-5) return vec2(10.0);
    vec2 now = clip_now.xy / clip_now.w * 0.5 + 0.5;
    vec2 prev = clip_prev.xy / clip_prev.w * 0.5 + 0.5;
    return clamp(now - prev, vec2(-10.0), vec2(10.0));
}

// Analytic single-segment transport for distant surfaces. The sky itself is already atmospheric.
vec3 aerial_transmittance(Frame f, vec3 world) {
    if (f.aerial.x <= 0.0) return vec3(1.0);
    float distance_m = max(0.0, length(world - f.camera_position.xyz) - f.aerial.y);
    return exp(-distance_m * f.aerial.x * vec3(0.75, 0.95, 1.25));
}

vec3 aerial_surface(Frame f, vec3 radiance, vec3 world) {
    if (f.aerial.x <= 0.0) return radiance;
    vec3 visibility = aerial_transmittance(f, world);
    return radiance * visibility + f.sky_color.rgb * (1.0 - visibility);
}

const uint FOG_SLICES = 16u;

vec4 fog_sample(Frame f, uint atlas, vec2 uv, float distance_m) {
    if (atlas == NO_TEXTURE) return vec4(0, 0, 0, 1);
    vec2 atlas_size = vec2(textureSize(sampler2D(astral_textures[atlas], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP]), 0));
    vec2 slice_size = atlas_size / vec2(1.0, float(FOG_SLICES));
    vec2 xy = clamp(uv * slice_size, vec2(0.5), slice_size - 0.5);
    float layer = sqrt(clamp(distance_m / f.fog.y, 0.0, 1.0)) * float(FOG_SLICES);
    float lower = floor(layer);
    float upper = min(lower + 1.0, float(FOG_SLICES));
    vec2 a = (xy + vec2(0, max(0.0, lower - 1.0) * slice_size.y)) / atlas_size;
    vec2 b = (xy + vec2(0, max(0.0, upper - 1.0) * slice_size.y)) / atlas_size;
    vec4 start = lower == 0.0 ? vec4(0, 0, 0, 1) :
        textureLod(sampler2D(astral_textures[atlas], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP]), a, 0.0);
    vec4 end = textureLod(sampler2D(astral_textures[atlas], astral_samplers[ASTRAL_SAMPLER_LINEAR_CLAMP]), b, 0.0);
    // Interpolate in optical distance rather than slice index for the quadratic grid.
    float fraction = (layer * layer - lower * lower) / max(1.0, upper * upper - lower * lower);
    return mix(start, end, fraction);
}

vec3 reconstruct_world(Frame f, vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = f.inv_view_projection * ndc;
    return world.xyz / world.w;
}

vec3 reconstruct_prev_world(Frame f, vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = f.inv_prev_view_projection * ndc;
    return world.xyz / world.w;
}

#endif
